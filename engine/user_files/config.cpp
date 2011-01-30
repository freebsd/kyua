// Copyright 2010, 2011 Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#if defined(HAVE_CONFIG_H)
#   include "config.h"
#endif

#include "engine/exceptions.hpp"
#include "engine/user_files/common.hpp"
#include "engine/user_files/config.hpp"
#include "utils/format/macros.hpp"
#include "utils/lua/exceptions.hpp"
#include "utils/lua/operations.hpp"
#include "utils/lua/wrap.ipp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"

namespace fs = utils::fs;
namespace lua = utils::lua;
namespace passwd = utils::passwd;
namespace user_files = engine::user_files;

using utils::none;
using utils::optional;


namespace {


/// An empty key/value map to use as a default return value.
static const user_files::properties_map empty_properties_map;


}  // anonymous namespace


// These namespace blocks are here to help Doxygen match the functions to their
// prototypes...
namespace engine {
namespace user_files {
namespace detail {


/// Gets a table of key/value string pairs from the Lua state.
///
/// \pre stack(-1) is the table to scan.
///
/// \param state The Lua state.
/// \param test_suite The name of the test suite to which these key/value pairs
///     belong.  For error reporting purposes only.
///
/// \return A map of key/value pairs.
///
/// \throw error If any of the keys or values is invalid.
properties_map
get_properties(lua::state& state, const std::string& test_suite)
{
    PRE(state.is_table());

    properties_map properties;

    lua::stack_cleaner cleaner(state);

    state.push_nil();
    while (state.next()) {
        if (!state.is_string(-2))
            throw engine::error(F("Found non-string property name for test "
                                  "suite '%s'") % test_suite);
        const std::string name = state.to_string(-2);

        std::string value;
        if (state.is_boolean(-1))
            value = state.to_boolean(-1) ? "true" : "false";
        else if (state.is_number(-1))
            value = state.to_string(-1);
        else if (state.is_string(-1))
            value = state.to_string(-1);
        else
            throw engine::error(F("Invalid value for property '%s' of test "
                                  "suite '%s': must be a boolean, a number "
                                  "or a string") % name % test_suite);

        INV(properties.find(name) == properties.end());
        properties[name] = value;

        state.pop(1);
    }

    return properties;
}


/// Queries an optional Lua string variable.
///
/// \param state The Lua state.
/// \param expr An expression to resolve the variable to query.
/// \param default_value The default value for the variable.
///
/// \return The value of 'expr', or default_value if 'expr' is nil.
///
/// \throw engine:error If the variable has an invalid type.
std::string
get_string_var(lua::state& state, const std::string& expr,
               const std::string& default_value)
{
    lua::stack_cleaner cleaner(state);
    lua::eval(state, expr);
    if (state.is_nil())
        return default_value;
    else if (state.is_string())
        return state.to_string();
    else
        throw engine::error(F("Invalid type for variable '%s': must be "
                              "a string") % expr);
}


/// Gets a mapping of test suite names to properties from the Lua state.
///
/// \param state The Lua state.
/// \param expr An expression to resolve the table to query.
///
/// \return A map of test suite names to key/value pairs.
///
/// \throw error If any of the keys or values is invalid.
test_suites_map
get_test_suites(lua::state& state, const std::string& expr)
{
    lua::stack_cleaner cleaner(state);

    lua::eval(state, expr);
    if (!state.is_table())
        throw engine::error(F("'%s' is not a table") % expr);

    test_suites_map test_suites;

    state.push_nil();
    while (state.next()) {
        if (!state.is_string(-2))
            throw engine::error(F("Found non-string test suite name in '%s'")
                                % expr);
        const std::string test_suite = state.to_string(-2);

        if (!state.is_table(-1))
            throw engine::error(F("Found non-table properties for test suite "
                                  "'%s'") % test_suite);
        INV(test_suites.find(test_suite) == test_suites.end());
        const properties_map properties = get_properties(state, test_suite);
        if (!properties.empty())
            test_suites[test_suite] = properties;
        state.pop(1);
    }

    return test_suites;
}


/// Queries a Lua variable that refers to an existent system user.
///
/// \param state The Lua state.
/// \param expr An expression to resolve the variable to query.
///
/// \return The user data if the variable is defined, or none if the variable
/// is nil.
///
/// \throw engine::error If the variable has an invalid type or if the specified
///     user cannot be found on the system.
optional< passwd::user >
get_user_var(lua::state& state, const std::string& expr)
{
    lua::stack_cleaner cleaner(state);
    lua::eval(state, expr);
    if (state.is_nil()) {
        return none;
    } else if (state.is_number()) {
        const int uid = state.to_integer();
        try {
            return utils::make_optional(passwd::find_user_by_uid(uid));
        } catch (const std::runtime_error& e) {
            throw engine::error(F("Cannot find user with UID %d defined in "
                                  "variable '%s'") % uid % expr);
        }
    } else if (state.is_string()) {
        const std::string name = state.to_string();
        try {
            return utils::make_optional(passwd::find_user_by_name(name));
        } catch (const std::runtime_error& e) {
            throw engine::error(F("Cannot find user with name '%s' defined in "
                                  "variable '%s'") % name % expr);
        }
    } else {
        throw engine::error(F("Invalid type for user variable '%s': must be "
                              "a UID or a user name") % expr);
    }
}


}  // namespace detail
}  // namespace user_files
}  // namespace engine


/// Constructs a config form initialized data.
///
/// Use load() to parse a configuration file and construct a config object.
///
/// \param architecture_ Name of the system architecture.
/// \param platform_ Name of the system platform.
/// \param unprivileged_user_ The unprivileged user, if any.
/// \param test_suites_ The configuration data for the test suites.
user_files::config::config(const std::string& architecture_,
                           const std::string& platform_,
                           const optional< passwd::user >& unprivileged_user_,
                           const test_suites_map& test_suites_) :
    architecture(architecture_),
    platform(platform_),
    unprivileged_user(unprivileged_user_),
    test_suites(test_suites_)
{
}


/// Constructs a config with the built-in settings.
user_files::config
user_files::config::defaults(void)
{
    return config(KYUA_ARCHITECTURE, KYUA_PLATFORM, none, test_suites_map());
}


/// Parses a test suite configuration file.
///
/// \param file The file to parse.
///
/// \return High-level representation of the configuration file.
///
/// \throw error If the file does not exist.  TODO(jmmv): This exception is not
///     accurate enough.
user_files::config
user_files::config::load(const utils::fs::path& file)
{
    config values = defaults();

    try {
        lua::state state;
        lua::stack_cleaner cleaner(state);

        const user_files::syntax_def syntax = user_files::do_user_file(
            state, file);
        if (syntax.first != "config")
            throw engine::error(F("Unexpected file format '%s'; "
                                  "need 'config'") % syntax.first);
        if (syntax.second != 1)
            throw engine::error(F("Unexpected file version '%d'; "
                                  "only 1 is supported") % syntax.second);

        values.architecture = detail::get_string_var(state, "architecture",
                                                     values.architecture);
        values.platform = detail::get_string_var(state, "platform",
                                                 values.platform);
        values.unprivileged_user = detail::get_user_var(
            state, "unprivileged_user");

        values.test_suites = detail::get_test_suites(state,
                                                     "config.TEST_SUITES");
    } catch (const lua::error& e) {
        throw engine::error(F("Load failed: %s") % e.what());
    }

    return values;
}


/// Looks up the configuration properties of a particular test suite.
///
/// This is just a convenience method to access the contents of the test_suite
/// member field.
///
/// \param name The name of the test suite being queried.
///
/// \return The properties for the test suite.  If the test suite has no
/// properties, returns an empty properties set.
const user_files::properties_map&
user_files::config::test_suite(const std::string& name) const
{
    const user_files::test_suites_map::const_iterator iter =
        test_suites.find(name);
    if (iter == test_suites.end())
        return empty_properties_map;
    else
        return (*iter).second;
}
