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
#include "engine/test_program.hpp"
#include "engine/test_result.hpp"
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

using utils::none;
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


/// Internal implementation of a test case.
struct engine::atf_iface::test_case::impl {
    /// The test case description.
    std::string description;

    /// Whether the test case has a cleanup routine or not.
    bool has_cleanup;

    /// The maximum amount of time the test case can run for.
    datetime::delta timeout;

    /// List of architectures in which the test case can run; empty = any.
    strings_set allowed_architectures;

    /// List of platforms in which the test case can run; empty = any.
    strings_set allowed_platforms;

    /// List of configuration variables needed by the test case.
    strings_set required_configs;

    /// List of files needed by the test case.
    paths_set required_files;

    /// List of programs needed by the test case.
    paths_set required_programs;

    /// Privileges required to run the test case.
    ///
    /// Can be empty, in which case means "any privileges", or any of "root" or
    /// "unprivileged".
    std::string required_user;

    /// User-defined meta-data properties.
    properties_map user_metadata;

    /// Fake result to return instead of running the test case.
    optional< test_result > fake_result;

    /// Constructor.
    ///
    /// \param description_ See the parent class.
    /// \param has_cleanup_ See the parent class.
    /// \param timeout_ See the parent class.
    /// \param allowed_architectures_ See the parent class.
    /// \param allowed_platforms_ See the parent class.
    /// \param required_configs_ See the parent class.
    /// \param required_files_ See the parent class.
    /// \param required_programs_ See the parent class.
    /// \param required_user_ See the parent class.
    /// \param user_metadata_ See the parent class.
    /// \param fake_result_ Fake result to return instead of running the test
    ///     case.
    impl(const std::string& description_,
         const bool has_cleanup_,
         const datetime::delta& timeout_,
         const strings_set& allowed_architectures_,
         const strings_set& allowed_platforms_,
         const strings_set& required_configs_,
         const paths_set& required_files_,
         const paths_set& required_programs_,
         const std::string& required_user_,
         const properties_map& user_metadata_,
         const optional< test_result >& fake_result_) :
        description(description_),
        has_cleanup(has_cleanup_),
        timeout(timeout_),
        allowed_architectures(allowed_architectures_),
        allowed_platforms(allowed_platforms_),
        required_configs(required_configs_),
        required_files(required_files_),
        required_programs(required_programs_),
        required_user(required_user_),
        user_metadata(user_metadata_),
        fake_result(fake_result_)
    {
        PRE(required_user.empty() || required_user == "unprivileged" ||
            required_user == "root");

        for (properties_map::const_iterator iter = user_metadata.begin();
             iter != user_metadata.end(); iter++) {
            const std::string& property_name = (*iter).first;
            PRE_MSG(property_name.size() > 2 &&
                    property_name.substr(0, 2) == "X-",
                    "User properties must be prefixed by X-");
        }
    }
};


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
    _pimpl(new impl(description_, has_cleanup_, timeout_,
                    allowed_architectures_, allowed_platforms_,
                    required_configs_, required_files_, required_programs_,
                    required_user_, user_metadata_, none))
{
}


/// Constructs a new fake test case.
///
/// A fake test case is a test case that is not really defined by the test
/// program.  Such test cases have a name surrounded by '__' and, when executed,
/// they return a fixed, pre-recorded result.  This functionality is used, for
/// example, to dynamically create a test case representing the test program
/// itself when it is broken (i.e. when it's even unable to provide a list of
/// its own test cases).
///
/// \param test_program_ The test program this test case belongs to.
/// \param name_ The name to give to this fake test case.  This name has to be
///     prefixed and suffixed by '__' to clearly denote that this is internal.
/// \param description_ The description of the test case, if any.
/// \param test_result_ The fake result to return when this test case is run.
atf_iface::test_case::test_case(const base_test_program& test_program_,
                                const std::string& name_,
                                const std::string& description_,
                                const engine::test_result& test_result_) :
    base_test_case(test_program_, name_),
    _pimpl(new impl(description_, false, default_timeout,
                    strings_set(), strings_set(), strings_set(), paths_set(),
                    paths_set(), "", properties_map(),
                    utils::make_optional(test_result_)))
{
    PRE_MSG(name_.length() > 4 && name_.substr(0, 2) == "__" &&
            name_.substr(name_.length() - 2) == "__",
            "Invalid fake name provided to fake test case");
}


/// Destructor.
atf_iface::test_case::~test_case(void)
{
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


/// Gets the description of the test case.
///
/// \return The description of the test case.
const std::string&
atf_iface::test_case::description(void) const
{
    return _pimpl->description;
}


/// Gets whether the test case has a cleanup routine or not.
///
/// \return True if the test case has a cleanup routine, false otherwise.
bool
atf_iface::test_case::has_cleanup(void) const
{
    return _pimpl->has_cleanup;
}


/// Gets the test case timeout.
///
/// \return The test case timeout.
const datetime::delta&
atf_iface::test_case::timeout(void) const
{
    return _pimpl->timeout;
}


/// Gets the list of allowed architectures.
///
/// \return The list of allowed architectures.
const atf_iface::strings_set&
atf_iface::test_case::allowed_architectures(void) const
{
    return _pimpl->allowed_architectures;
}


/// Gets the list of allowed platforms.
///
/// \return The list of allowed platforms.
const atf_iface::strings_set&
atf_iface::test_case::allowed_platforms(void) const
{
    return _pimpl->allowed_platforms;
}


/// Gets the list of required configuration variables.
///
/// \return The list of required configuration variables.
const atf_iface::strings_set&
atf_iface::test_case::required_configs(void) const
{
    return _pimpl->required_configs;
}


/// Gets the list of required files.
///
/// \return The list of required files.
const atf_iface::paths_set&
atf_iface::test_case::required_files(void) const
{
    return _pimpl->required_files;
}


/// Gets the list of required programs.
///
/// \return The list of required programs.
const atf_iface::paths_set&
atf_iface::test_case::required_programs(void) const
{
    return _pimpl->required_programs;
}


/// Gets the required user name.
///
/// \return The required user name.
const std::string&
atf_iface::test_case::required_user(void) const
{
    return _pimpl->required_user;
}


/// Gets the custom user metadata, if any.
///
/// \return The user metadata.
const engine::properties_map&
atf_iface::test_case::user_metadata(void) const
{
    return _pimpl->user_metadata;
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
    properties_map props = _pimpl->user_metadata;

    if (!_pimpl->description.empty())
        props["descr"] = _pimpl->description;
    if (_pimpl->has_cleanup)
        props["has.cleanup"] = "true";
    if (_pimpl->timeout != default_timeout) {
        INV(_pimpl->timeout.useconds == 0);
        props["timeout"] = F("%d") % _pimpl->timeout.seconds;
    }
    if (!_pimpl->allowed_architectures.empty())
        props["require.arch"] = flatten_set(_pimpl->allowed_architectures);
    if (!_pimpl->allowed_platforms.empty())
        props["require.machine"] = flatten_set(_pimpl->allowed_platforms);
    if (!_pimpl->required_configs.empty())
        props["require.config"] = flatten_set(_pimpl->required_configs);
    if (!_pimpl->required_files.empty())
        props["require.files"] = flatten_set(_pimpl->required_files);
    if (!_pimpl->required_programs.empty())
        props["require.progs"] = flatten_set(_pimpl->required_programs);
    if (!_pimpl->required_user.empty())
        props["require.user"] = _pimpl->required_user;

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
    return
        test_program().relative_path() == tc.test_program().relative_path() &&
        name() == tc.name() &&
        _pimpl->description == tc._pimpl->description &&
        _pimpl->has_cleanup == tc._pimpl->has_cleanup &&
        _pimpl->allowed_architectures == tc._pimpl->allowed_architectures &&
        _pimpl->allowed_platforms == tc._pimpl->allowed_platforms &&
        _pimpl->required_configs == tc._pimpl->required_configs &&
        _pimpl->required_files == tc._pimpl->required_files &&
        _pimpl->required_programs == tc._pimpl->required_programs &&
        _pimpl->required_user == tc._pimpl->required_user &&
        _pimpl->timeout == tc._pimpl->timeout &&
        _pimpl->user_metadata == tc._pimpl->user_metadata;
}


/// Checks if all the requirements specified by the test case are met.
///
/// \param config The engine configuration.
///
/// \return A string describing what is missing; empty if everything is OK.
std::string
atf_iface::test_case::check_requirements(const user_files::config& config) const
{
    for (strings_set::const_iterator iter = _pimpl->required_configs.begin();
         iter != _pimpl->required_configs.end(); iter++) {
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

    if (!_pimpl->allowed_architectures.empty()) {
        if (_pimpl->allowed_architectures.find(config.architecture) ==
            _pimpl->allowed_architectures.end())
            return F("Current architecture '%s' not supported") %
                config.architecture;
    }

    if (!_pimpl->allowed_platforms.empty()) {
        if (_pimpl->allowed_platforms.find(config.platform) ==
            _pimpl->allowed_platforms.end())
            return F("Current platform '%s' not supported") % config.platform;
    }

    if (!_pimpl->required_user.empty()) {
        const passwd::user user = passwd::current_user();
        if (_pimpl->required_user == "root") {
            if (!user.is_root())
                return "Requires root privileges";
        } else if (_pimpl->required_user == "unprivileged") {
            if (user.is_root())
                if (!config.unprivileged_user)
                    return "Requires an unprivileged user but the "
                        "unprivileged-user configuration variable is not "
                        "defined";
        } else
            UNREACHABLE_MSG("Value of require.user not properly validated");
    }

    for (paths_set::const_iterator iter = _pimpl->required_files.begin();
         iter != _pimpl->required_files.end(); iter++) {
        INV((*iter).is_absolute());
        if (!fs::exists(*iter))
            return F("Required file '%s' not found") % *iter;
    }

    for (paths_set::const_iterator iter = _pimpl->required_programs.begin();
         iter != _pimpl->required_programs.end(); iter++) {
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
/// \param hooks Hooks to introspect the execution of the test case.
/// \param stdout_path The file to which to redirect the stdout of the test.
///     If none, use a temporary file in the work directory.
/// \param stderr_path The file to which to redirect the stdout of the test.
///     If none, use a temporary file in the work directory.
///
/// \return The result of the execution.
engine::test_result
atf_iface::test_case::execute(const user_files::config& config,
                              test_case_hooks& hooks,
                              const optional< fs::path >& stdout_path,
                              const optional< fs::path >& stderr_path) const
{
    if (_pimpl->fake_result)
        return _pimpl->fake_result.get();
    else
        return run_test_case(*this, config, hooks, stdout_path, stderr_path);
}
