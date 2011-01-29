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


// These namespace blocks are here to help Doxygen match the functions to their
// prototypes...
namespace engine {
namespace user_files {
namespace detail {


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
user_files::config::config(const std::string& architecture_,
                           const std::string& platform_,
                           const optional< passwd::user >& unprivileged_user_) :
    architecture(architecture_),
    platform(platform_),
    unprivileged_user(unprivileged_user_)
{
}


/// Constructs a config with the built-in settings.
user_files::config
user_files::config::defaults(void)
{
    return config(KYUA_ARCHITECTURE, KYUA_PLATFORM, none);
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
    } catch (const lua::error& e) {
        throw engine::error(F("Load failed: %s") % e.what());
    }

    return values;
}
