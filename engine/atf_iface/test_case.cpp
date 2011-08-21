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

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <sstream>

#include "engine/atf_iface/runner.hpp"
#include "engine/atf_iface/test_case.hpp"
#include "engine/exceptions.hpp"
#include "engine/results.hpp"
#include "engine/test_program.hpp"
#include "engine/user_files/config.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/format/macros.hpp"
#include "utils/passwd.hpp"
#include "utils/sanity.hpp"

namespace atf_iface = engine::atf_iface;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace user_files = engine::user_files;

using utils::optional;


namespace {


/// The default timeout value for test cases that do not provide one.
/// TODO(jmmv): We should not be doing this; see issue 5 for details.
static datetime::delta default_timeout(300, 0);


/// Concatenates a collection of objects in a string using ' ' as a separator.
///
/// \param set The objects to join.  This cannot be empty.
///
/// \return The concatenation of all the objects in the set.
template< class T >
std::string
flatten_set(const std::set< T >& set)
{
    PRE(!set.empty());

    std::ostringstream output;
    std::copy(set.begin(), set.end(), std::ostream_iterator< T >(output, " "));

    std::string result = output.str();
    result.erase(result.end() - 1);
    return result;
}


}  // anonymous namespace


/// Parses a boolean property.
///
/// \param name The name of the property; used for error messages.
/// \param value The textual value to process.
///
/// \return The value as a boolean.
///
/// \throw engine::format_error If the value is invalid.
bool
engine::atf_iface::detail::parse_bool(const std::string& name,
                                      const std::string& value)
{
    if (value == "true" || value == "yes")
        return true;
    else if (value == "false" || value == "no")
        return false;
    else
        throw format_error(F("Invalid value '%s' for boolean property '%s'") %
                           value % name);
}


/// Parses a whitespace-separated list property.
///
/// \param name The name of the property; used for error messages.
/// \param value The textual value to process.
///
/// \return The value as a collection of strings.
///
/// \throw engine::format_error If the value is invalid.
engine::atf_iface::strings_set
engine::atf_iface::detail::parse_list(const std::string& name,
                                      const std::string& value)
{
    strings_set words;

    {
        std::istringstream input(value);
        std::copy(std::istream_iterator< std::string >(input),
                  std::istream_iterator< std::string >(),
                  std::insert_iterator< strings_set >(words, words.begin()));
    }

    if (words.empty())
        throw format_error(F("Invalid empty value for list property '%s'") %
                           name);

    return words;
}


/// Parses an integer property.
///
/// \param name The name of the property; used for error messages.
/// \param value The textual value to process.
///
/// \return The value as an integer.
///
/// \throw engine::format_error If the value is invalid.
unsigned long
engine::atf_iface::detail::parse_ulong(const std::string& name,
                                       const std::string& value)
{
    if (value.empty())
        throw format_error(F("Invalid empty value for integer property '%s'") %
                           name);

    char* endptr;
    const unsigned long l = std::strtoul(value.c_str(), &endptr, 10);
    if (value.find_first_of("- \t") != std::string::npos || *endptr != '\0' ||
        (l == 0 && errno == EINVAL) ||
        (l == std::numeric_limits< unsigned long >::max() && errno == ERANGE))
        throw format_error(F("Invalid value '%s' for integer property '%s'") %
                           value % name);
    return l;
}


/// Parses a list of files (as given through 'require.files').
///
/// \param name The name of the property; used for error messages.
/// \param value The textual value to process.
///
/// \return The value as an integer.
///
/// \throw engine::format_error If any of the files in the list is invalid
///     or if the list itself is invalid.
engine::atf_iface::paths_set
engine::atf_iface::detail::parse_require_files(const std::string& name,
                                               const std::string& value)
{
    std::set< fs::path > files;

    const strings_set raw_files = parse_list(name, value);
    for (strings_set::const_iterator iter = raw_files.begin();
         iter != raw_files.end(); iter++) {
        try {
            const fs::path file(*iter);
            if (!file.is_absolute())
                throw format_error(F("Relative path '%s' not allowed in "
                                     "property '%s'") % *iter % name);
            files.insert(file);
        } catch (const fs::invalid_path_error& e) {
            throw format_error(F("Invalid path '%s' in property '%s'") %
                               *iter % name);
        }
    }

    return files;
}


/// Parses a list of program names (as given through 'require.progs').
///
/// \param name The name of the property; used for error messages.
/// \param value The textual value to process.
///
/// \return The value as an integer.
///
/// \throw engine::format_error If any of the programs in the list is invalid
///     or if the list itself is invalid.
engine::atf_iface::paths_set
engine::atf_iface::detail::parse_require_progs(const std::string& name,
                                               const std::string& value)
{
    std::set< fs::path > programs;

    const strings_set raw_programs = parse_list(name, value);
    for (strings_set::const_iterator iter = raw_programs.begin();
         iter != raw_programs.end(); iter++) {
        try {
            const fs::path program(*iter);
            if (!program.is_absolute() && program.str() != program.leaf_name())
                throw format_error(F("Relative path '%s' not allowed in "
                                     "property '%s'") % *iter % name);
            programs.insert(program);
        } catch (const fs::invalid_path_error& e) {
            throw format_error(F("Invalid path '%s' in property '%s'") %
                               *iter % name);
        }
    }

    return programs;
}


/// Parses the required user (as given through 'require.user').
///
/// \param name The name of the property; used for error messages.
/// \param value The textual value to process.
///
/// \return The value as an integer.
///
/// \throw engine::format_error If the given value is invalid.
std::string
engine::atf_iface::detail::parse_require_user(const std::string& name,
                                              const std::string& value)
{
    if (value.empty() || value == "root" || value == "unprivileged")
        return value;
    else
        throw format_error(F("Invalid user '%s' for property '%s'") %
                           value % name);
}


/// Constructs a new test case.
///
/// \param test_program_ The test program this test case belongs to.  This
///     object must exist during the lifetime of the test case.
/// \param name_ The name of the test case.
/// \param description_ The description of the test case, if any.
/// \param has_cleanup_ Whether the test case has a cleanup routine or not.
/// \param timeout_ The maximum time the test case can run for.
/// \param allowed_architectures_ List of architectures on which this test case
///     can run.  If empty, all architectures are valid.
/// \param allowed_platforms_ List of platforms on which this test case can run.
///     If empty, all platforms types are valid.
/// \param required_configs_ List of configuration variables that must be
///     defined to run this test case.
/// \param required_files_ List of files required by the test case.
/// \param required_programs_ List of programs required by the test case.
/// \param required_user_ The user required to run this test case.  Can be
///     empty, in which case any user is allowed, or any of 'unprivileged' or
///     'root'.
/// \param user_metadata_ User-defined meta-data properties.  The names of all
///     of these properties must start by 'X-'.
atf_iface::test_case::test_case(const base_test_program& test_program_,
                                const std::string& name_,
                                const std::string& description_,
                                const bool has_cleanup_,
                                const datetime::delta& timeout_,
                                const strings_set& allowed_architectures_,
                                const strings_set& allowed_platforms_,
                                const strings_set& required_configs_,
                                const paths_set& required_files_,
                                const paths_set& required_programs_,
                                const std::string& required_user_,
                                const properties_map& user_metadata_) :
    base_test_case(test_program_, name_),
    description(description_),
    has_cleanup(has_cleanup_),
    timeout(timeout_),
    allowed_architectures(allowed_architectures_),
    allowed_platforms(allowed_platforms_),
    required_configs(required_configs_),
    required_files(required_files_),
    required_programs(required_programs_),
    required_user(required_user_),
    user_metadata(user_metadata_)
{
    PRE(required_user_.empty() || required_user_ == "unprivileged" ||
        required_user_ == "root");

    for (properties_map::const_iterator iter = user_metadata_.begin();
         iter != user_metadata_.end(); iter++) {
        const std::string& property_name = (*iter).first;
        PRE_MSG(property_name.size() > 2 && property_name.substr(0, 2) == "X-",
                "User properties must be prefixed by X-");
    }
}


/// Creates a test case from a set of raw properties (the test program output).
///
/// \param test_program_ The test program this test case belongs to.  This
///     object must exist during the lifetime of the test case.
/// \param name_ The name of the test case.
/// \param raw_properties The properties (name/value string pairs) as provided
///     by the test program.
///
/// \return A new test_case.
///
/// \throw engine::format_error If the syntax of any of the properties is
///     invalid.
atf_iface::test_case
atf_iface::test_case::from_properties(const base_test_program& test_program_,
                                      const std::string& name_,
                                      const properties_map& raw_properties)
{
    std::string description_;
    bool has_cleanup_ = false;
    datetime::delta timeout_ = default_timeout;
    strings_set allowed_architectures_;
    strings_set allowed_platforms_;
    strings_set required_configs_;
    paths_set required_files_;
    paths_set required_programs_;
    std::string required_user_;
    properties_map user_metadata_;

    for (properties_map::const_iterator iter = raw_properties.begin();
         iter != raw_properties.end(); iter++) {
        const std::string& name = (*iter).first;
        const std::string& value = (*iter).second;

        if (name == "descr") {
            description_ = value;
        } else if (name == "has.cleanup") {
            has_cleanup_ = detail::parse_bool(name, value);
        } else if (name == "require.arch") {
            allowed_architectures_ = detail::parse_list(name, value);
        } else if (name == "require.config") {
            required_configs_ = detail::parse_list(name, value);
        } else if (name == "require.files") {
            required_files_ = detail::parse_require_files(name, value);
        } else if (name == "require.machine") {
            allowed_platforms_ = detail::parse_list(name, value);
        } else if (name == "require.progs") {
            required_programs_ = detail::parse_require_progs(name, value);
        } else if (name == "require.user") {
            required_user_ = detail::parse_require_user(name, value);
        } else if (name == "timeout") {
            timeout_ = datetime::delta(detail::parse_ulong(name, value), 0);
        } else if (name.length() > 2 && name.substr(0, 2) == "X-") {
            user_metadata_[name] = value;
        } else {
            throw engine::format_error(F("Unknown test case metadata property "
                                         "'%s'") % name);
        }
    }

    return test_case(test_program_, name_, description_, has_cleanup_,
                     timeout_, allowed_architectures_, allowed_platforms_,
                     required_configs_, required_files_, required_programs_,
                     required_user_, user_metadata_);
}


/// Returns a string representation of all test case properties.
///
/// The returned keys and values match those that can be defined by the test
/// case.
///
/// \return A key/value mapping describing all the test case properties.
engine::properties_map
atf_iface::test_case::get_all_properties(void) const
{
    properties_map props = user_metadata;

    if (!description.empty())
        props["descr"] = description;
    if (has_cleanup)
        props["has.cleanup"] = "true";
    if (timeout != default_timeout) {
        INV(timeout.useconds == 0);
        props["timeout"] = F("%d") % timeout.seconds;
    }
    if (!allowed_architectures.empty())
        props["require.arch"] = flatten_set(allowed_architectures);
    if (!allowed_platforms.empty())
        props["require.machine"] = flatten_set(allowed_platforms);
    if (!required_configs.empty())
        props["require.config"] = flatten_set(required_configs);
    if (!required_files.empty())
        props["require.files"] = flatten_set(required_files);
    if (!required_programs.empty())
        props["require.progs"] = flatten_set(required_programs);
    if (!required_user.empty())
        props["require.user"] = required_user;

    return props;
}


/// Equality comparator.
///
/// \todo It looks like this is only used for testing purposes.  Maybe we should
/// just get rid of this.
///
/// \param tc The test case to compare this test case to.
///
/// \return bool True if the test cases are equal, false otherwise.
bool
atf_iface::test_case::operator==(const test_case& tc) const
{
    return identifier() == tc.identifier() &&
        description == tc.description &&
        has_cleanup == tc.has_cleanup &&
        allowed_architectures == tc.allowed_architectures &&
        allowed_platforms == tc.allowed_platforms &&
        required_configs == tc.required_configs &&
        required_files == tc.required_files &&
        required_programs == tc.required_programs &&
        required_user == tc.required_user &&
        timeout == tc.timeout &&
        user_metadata == tc.user_metadata;
}


/// Checks if all the requirements specified by the test case are met.
///
/// \param config The engine configuration.
///
/// \return A string describing what is missing; empty if everything is OK.
std::string
atf_iface::test_case::check_requirements(const user_files::config& config) const
{
    for (strings_set::const_iterator iter = required_configs.begin();
         iter != required_configs.end(); iter++) {
        const user_files::properties_map& properties = config.test_suite(
            test_program().test_suite_name());
        if (*iter == "unprivileged-user") {
            if (!config.unprivileged_user)
                return F("Required configuration property '%s' not defined") %
                    *iter;
        } else if (properties.find(*iter) == properties.end())
            return F("Required configuration property '%s' not defined") %
                *iter;
    }

    if (!allowed_architectures.empty()) {
        if (allowed_architectures.find(config.architecture) ==
            allowed_architectures.end())
            return F("Current architecture '%s' not supported") %
                config.architecture;
    }

    if (!allowed_platforms.empty()) {
        if (allowed_platforms.find(config.platform) == allowed_platforms.end())
            return F("Current platform '%s' not supported") % config.platform;
    }

    if (!required_user.empty()) {
        const passwd::user user = passwd::current_user();
        if (required_user == "root") {
            if (!user.is_root())
                return "Requires root privileges";
        } else if (required_user == "unprivileged") {
            if (user.is_root())
                if (!config.unprivileged_user)
                    return "Requires an unprivileged user but the "
                        "unprivileged-user configuration variable is not "
                        "defined";
        } else
            UNREACHABLE_MSG("Value of require.user not properly validated");
    }

    for (paths_set::const_iterator iter = required_files.begin();
         iter != required_files.end(); iter++) {
        INV((*iter).is_absolute());
        if (!fs::exists(*iter))
            return F("Required file '%s' not found") % *iter;
    }

    for (paths_set::const_iterator iter = required_programs.begin();
         iter != required_programs.end(); iter++) {
        if ((*iter).is_absolute()) {
            if (!fs::exists(*iter))
                return F("Required program '%s' not found") % *iter;
        } else {
            if (!fs::find_in_path((*iter).c_str()))
                return F("Required program '%s' not found in PATH") % *iter;
        }
    }

    return "";
}


/// Executes the test case.
///
/// This should not throw any exception: problems detected during execution are
/// reported as a broken test case result.
///
/// \param config The run-time configuration for the test case.
///
/// \return The result of the execution.
engine::results::result_ptr
atf_iface::test_case::execute(const user_files::config& config,
                              const optional< fs::path >& stdout_path,
                              const optional< fs::path >& stderr_path) const
{
    return run_test_case(*this, config, stdout_path, stderr_path);
}
