// Copyright 2011 Google Inc.
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

#include <fstream>

#include <atf-c++.hpp>

#include "cli/common.hpp"
#include "engine/exceptions.hpp"
#include "engine/test_case.hpp"
#include "engine/user_files/config.hpp"
#include "engine/user_files/kyuafile.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/test_utils.hpp"

namespace cmdline = utils::cmdline;
namespace fs = utils::fs;
namespace user_files = engine::user_files;


namespace {


/// Creates a configuration file for testing purposes.
///
/// To ensure that the loaded file is the one created by this function, use
/// validate_mock_config().
///
/// \param name The name of the configuration file to create.
/// \param name The magic value to set in the configuration file, or NULL if a
///     broken configuration file is desired.
static void
create_mock_config(const char* name, const char* cookie)
{
    std::ofstream output(name);
    ATF_REQUIRE(output);
    if (cookie != NULL) {
        output << "syntax('config', 1)\n";
        output << "test_suite_var('suite', 'magic-value', '" << cookie
               << "')\n";
    } else {
        output << "syntax('invalid-file', 1)\n";
    }
}


/// Creates a Kyuafile for testing purposes.
///
/// To ensure that the loaded file is the one created by this function, use
/// validate_mock_kyuafile().
///
/// \param name The name of the configuration file to create.
/// \param cookie The magic value to set in the configuration file, or NULL if a
///     broken configuration file is desired.
static void
create_mock_kyuafile(const char* name, const char* cookie)
{
    std::ofstream output(name);
    ATF_REQUIRE(output);
    if (cookie != NULL) {
        output << "syntax('kyuafile', 1)\n";
        utils::create_file(fs::path(cookie));
        output << "atf_test_program{name='" << cookie << "', test_suite='a'}\n";
    } else {
        output << "syntax('invalid-file', 1)\n";
    }
}


/// Creates a test case identifier; provided to simplify typing.
///
/// \param test_program_name The test program name.
/// \param test_case_name The test case name.
static inline engine::test_case_id
make_id(const std::string& test_program_name, const std::string& test_case_name)
{
    return engine::test_case_id(fs::path(test_program_name), test_case_name);
}


/// Creates an invalid system configuration.
///
/// \param name The magic value to set in the configuration file, or NULL if a
///     broken configuration file is desired.
static void
mock_system_config(const char* cookie)
{
    fs::mkdir(fs::path("system-dir"), 0755);
    cli::set_confdir_for_testing(fs::current_path() / "system-dir");
    create_mock_config("system-dir/kyua.conf", cookie);
}


/// Creates an invalid user configuration.
///
/// \param name The magic value to set in the configuration file, or NULL if a
///     broken configuration file is desired.
static void
mock_user_config(const char* cookie)
{
    fs::mkdir(fs::path("user-dir"), 0755);
    utils::setenv("HOME", (fs::current_path() / "user-dir").str());
    create_mock_config("user-dir/.kyuarc", cookie);
}


/// Ensures that a loaded configuration was created with create_mock_config().
///
/// \param config The configuration to validate.
/// \param cookie The magic value to expect in the configuration file.
static void
validate_mock_config(const user_files::config& config, const char* cookie)
{
    const user_files::properties_map& properties = config.test_suite("suite");
    const user_files::properties_map::const_iterator iter =
        properties.find("magic-value");
    ATF_REQUIRE(iter != properties.end());
    ATF_REQUIRE_EQ(cookie, (*iter).second);
}


/// Ensures that a loaded configuration was created with create_mock_kyuafile().
///
/// \param config The configuration to validate.
/// \param cookie The magic value to expect in the configuration file.
static void
validate_mock_kyuafile(const user_files::kyuafile& kyuafile, const char* cookie)
{
    const user_files::test_programs_vector& test_programs =
        kyuafile.test_programs();
    ATF_REQUIRE_EQ(1, test_programs.size());
    ATF_REQUIRE_EQ(cookie, test_programs[0].binary_path.str());
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(load_config__none);
ATF_TEST_CASE_BODY(load_config__none)
{
    cli::set_confdir_for_testing(fs::path("/the/system/does/not/exist"));
    utils::setenv("HOME", "/the/user/does/not/exist");

    std::map< std::string, std::string > options;
    options["config"] = cli::config_option.default_value();
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    const user_files::config config = cli::load_config(mock_cmdline);
    ATF_REQUIRE(config.test_suites.empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(load_config__explicit__ok);
ATF_TEST_CASE_BODY(load_config__explicit__ok)
{
    mock_system_config(NULL);
    mock_user_config(NULL);

    create_mock_config("test-file", "hello");

    std::map< std::string, std::string > options;
    options["config"] = "test-file";
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    const user_files::config config = cli::load_config(mock_cmdline);
    validate_mock_config(config, "hello");
}


ATF_TEST_CASE_WITHOUT_HEAD(load_config__explicit__fail);
ATF_TEST_CASE_BODY(load_config__explicit__fail)
{
    mock_system_config("ok1");
    mock_user_config("ok2");

    create_mock_config("test-file", NULL);

    std::map< std::string, std::string > options;
    options["config"] = "test-file";
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    ATF_REQUIRE_THROW_RE(engine::error, "invalid-file",
                         cli::load_config(mock_cmdline));
}


ATF_TEST_CASE_WITHOUT_HEAD(load_config__user__ok);
ATF_TEST_CASE_BODY(load_config__user__ok)
{
    mock_system_config(NULL);
    mock_user_config("I am the user config");

    std::map< std::string, std::string > options;
    options["config"] = cli::config_option.default_value();
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    const user_files::config config = cli::load_config(mock_cmdline);
    validate_mock_config(config, "I am the user config");
}


ATF_TEST_CASE_WITHOUT_HEAD(load_config__user__fail);
ATF_TEST_CASE_BODY(load_config__user__fail)
{
    mock_system_config("valid");
    mock_user_config(NULL);

    std::map< std::string, std::string > options;
    options["config"] = cli::config_option.default_value();
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    ATF_REQUIRE_THROW_RE(engine::error, "invalid-file",
                         cli::load_config(mock_cmdline));
}


ATF_TEST_CASE_WITHOUT_HEAD(load_config__user__bad_home);
ATF_TEST_CASE_BODY(load_config__user__bad_home)
{
    mock_system_config("Fallback system config");
    utils::setenv("HOME", "");

    std::map< std::string, std::string > options;
    options["config"] = cli::config_option.default_value();
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    const user_files::config config = cli::load_config(mock_cmdline);
    validate_mock_config(config, "Fallback system config");
}


ATF_TEST_CASE_WITHOUT_HEAD(load_config__system__ok);
ATF_TEST_CASE_BODY(load_config__system__ok)
{
    mock_system_config("I am the system config");
    utils::setenv("HOME", "/the/user/does/not/exist");

    std::map< std::string, std::string > options;
    options["config"] = cli::config_option.default_value();
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    const user_files::config config = cli::load_config(mock_cmdline);
    validate_mock_config(config, "I am the system config");
}


ATF_TEST_CASE_WITHOUT_HEAD(load_config__system__fail);
ATF_TEST_CASE_BODY(load_config__system__fail)
{
    mock_system_config(NULL);
    utils::setenv("HOME", "/the/user/does/not/exist");

    std::map< std::string, std::string > options;
    options["config"] = cli::config_option.default_value();
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    ATF_REQUIRE_THROW_RE(engine::error, "invalid-file",
                         cli::load_config(mock_cmdline));
}


ATF_TEST_CASE_WITHOUT_HEAD(load_kyuafile__default);
ATF_TEST_CASE_BODY(load_kyuafile__default)
{
    std::map< std::string, std::string > options;
    options["kyuafile"] = cli::kyuafile_option.default_value();
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    create_mock_kyuafile("Kyuafile", "foo bar");
    const user_files::kyuafile config = cli::load_kyuafile(mock_cmdline);
    validate_mock_kyuafile(config, "foo bar");
}


ATF_TEST_CASE_WITHOUT_HEAD(load_kyuafile__explicit);
ATF_TEST_CASE_BODY(load_kyuafile__explicit)
{
    std::map< std::string, std::string > options;
    options["kyuafile"] = "another";
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    create_mock_kyuafile("Kyuafile", "no no no");
    create_mock_kyuafile("another", "yes yes yes");
    const user_files::kyuafile config = cli::load_kyuafile(mock_cmdline);
    validate_mock_kyuafile(config, "yes yes yes");
}


ATF_TEST_CASE_WITHOUT_HEAD(load_kyuafile__fail);
ATF_TEST_CASE_BODY(load_kyuafile__fail)
{
    std::map< std::string, std::string > options;
    options["kyuafile"] = "missing-file";
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    create_mock_kyuafile("Kyuafile", "no no no");
    ATF_REQUIRE_THROW_RE(engine::error, "missing-file",
                         cli::load_kyuafile(mock_cmdline));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__ctor__ok)
ATF_TEST_CASE_BODY(test_filters__ctor__ok)
{
    std::vector< std::string > args;
    args.push_back("foo/bar/baz");
    args.push_back("foo/bar:abc");
    args.push_back("bar:baz");
    args.push_back("baz");
    cli::test_filters unused_filters = cli::test_filters(args);
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__ctor__fail)
ATF_TEST_CASE_BODY(test_filters__ctor__fail)
{
    std::vector< std::string > args;
    args.push_back("i-am-good:yes");
    args.push_back("i-am-invalid:");
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "Test case.*'i-am-invalid:'",
                         (void)cli::test_filters(args));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__parse_user_filter__ok);
ATF_TEST_CASE_BODY(test_filters__parse_user_filter__ok)
{
    const cli::test_filters::filter_pair filter =
        cli::test_filters::parse_user_filter("foo");
    ATF_REQUIRE_EQ(fs::path("foo"), filter.first);
    ATF_REQUIRE(filter.second.empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__parse_user_filter__empty);
ATF_TEST_CASE_BODY(test_filters__parse_user_filter__empty)
{
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "empty",
                         cli::test_filters::parse_user_filter(""));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__parse_user_filter__absolute);
ATF_TEST_CASE_BODY(test_filters__parse_user_filter__absolute)
{
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "'/foo/bar'.*relative",
                         cli::test_filters::parse_user_filter("/foo//bar"));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__parse_user_filter__bad_program_name);
ATF_TEST_CASE_BODY(test_filters__parse_user_filter__bad_program_name)
{
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "Program name.*':foo'",
                         cli::test_filters::parse_user_filter(":foo"));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__parse_user_filter__bad_test_case);
ATF_TEST_CASE_BODY(test_filters__parse_user_filter__bad_test_case)
{
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "Test case.*'bar/baz:'",
                         cli::test_filters::parse_user_filter("bar/baz:"));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__parse_user_filter__bad_path);
ATF_TEST_CASE_BODY(test_filters__parse_user_filter__bad_path)
{
    // TODO(jmmv): Not implemented.  At the moment, the only reason for a path
    // to be invalid is if it is empty... but we are checking this exact
    // condition ourselves as part of the input validation.  So we can't mock in
    // an argument with an invalid non-empty path...
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__match_test_case__no_filters)
ATF_TEST_CASE_BODY(test_filters__match_test_case__no_filters)
{
    const std::vector< std::string > args;

    const cli::test_filters filters(args);
    ATF_REQUIRE(filters.match_test_case(make_id("foo", "baz")));
    ATF_REQUIRE(filters.match_test_case(make_id("foo/bar", "baz")));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__match_test_case__some_filters)
ATF_TEST_CASE_BODY(test_filters__match_test_case__some_filters)
{
    std::vector< std::string > args;
    args.push_back("top_test");
    args.push_back("subdir_1");
    args.push_back("subdir_2/a_test");
    args.push_back("subdir_2/b_test:foo");

    const cli::test_filters filters(args);
    ATF_REQUIRE( filters.match_test_case(make_id("top_test", "a")));
    ATF_REQUIRE( filters.match_test_case(make_id("subdir_1/foo", "a")));
    ATF_REQUIRE( filters.match_test_case(make_id("subdir_1/bar", "z")));
    ATF_REQUIRE( filters.match_test_case(make_id("subdir_2/a_test", "bar")));
    ATF_REQUIRE( filters.match_test_case(make_id("subdir_2/b_test", "foo")));
    ATF_REQUIRE(!filters.match_test_case(make_id("subdir_2/b_test", "bar")));
    ATF_REQUIRE(!filters.match_test_case(make_id("subdir_2/c_test", "foo")));
    ATF_REQUIRE(!filters.match_test_case(make_id("subdir_3", "hello")));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__match_test_program__no_filters)
ATF_TEST_CASE_BODY(test_filters__match_test_program__no_filters)
{
    const std::vector< std::string > args;

    const cli::test_filters filters(args);
    ATF_REQUIRE(filters.match_test_program(fs::path("foo")));
    ATF_REQUIRE(filters.match_test_program(fs::path("foo/bar")));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__match_test_program__some_filters)
ATF_TEST_CASE_BODY(test_filters__match_test_program__some_filters)
{
    std::vector< std::string > args;
    args.push_back("top_test");
    args.push_back("subdir_1");
    args.push_back("subdir_2/a_test");
    args.push_back("subdir_2/b_test:foo");

    const cli::test_filters filters(args);
    ATF_REQUIRE( filters.match_test_program(fs::path("top_test")));
    ATF_REQUIRE( filters.match_test_program(fs::path("subdir_1/foo")));
    ATF_REQUIRE( filters.match_test_program(fs::path("subdir_1/bar")));
    ATF_REQUIRE( filters.match_test_program(fs::path("subdir_2/a_test")));
    ATF_REQUIRE( filters.match_test_program(fs::path("subdir_2/b_test")));
    ATF_REQUIRE(!filters.match_test_program(fs::path("subdir_2/c_test")));
    ATF_REQUIRE(!filters.match_test_program(fs::path("subdir_3")));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, load_config__none);
    ATF_ADD_TEST_CASE(tcs, load_config__explicit__ok);
    ATF_ADD_TEST_CASE(tcs, load_config__explicit__fail);
    ATF_ADD_TEST_CASE(tcs, load_config__user__ok);
    ATF_ADD_TEST_CASE(tcs, load_config__user__fail);
    ATF_ADD_TEST_CASE(tcs, load_config__user__bad_home);
    ATF_ADD_TEST_CASE(tcs, load_config__system__ok);
    ATF_ADD_TEST_CASE(tcs, load_config__system__fail);

    ATF_ADD_TEST_CASE(tcs, load_kyuafile__default);
    ATF_ADD_TEST_CASE(tcs, load_kyuafile__explicit);
    ATF_ADD_TEST_CASE(tcs, load_kyuafile__fail);

    ATF_ADD_TEST_CASE(tcs, test_filters__ctor__ok);
    ATF_ADD_TEST_CASE(tcs, test_filters__ctor__fail);
    ATF_ADD_TEST_CASE(tcs, test_filters__parse_user_filter__ok);
    ATF_ADD_TEST_CASE(tcs, test_filters__parse_user_filter__empty);
    ATF_ADD_TEST_CASE(tcs, test_filters__parse_user_filter__absolute);
    ATF_ADD_TEST_CASE(tcs, test_filters__parse_user_filter__bad_program_name);
    ATF_ADD_TEST_CASE(tcs, test_filters__parse_user_filter__bad_test_case);
    ATF_ADD_TEST_CASE(tcs, test_filters__parse_user_filter__bad_path);
    ATF_ADD_TEST_CASE(tcs, test_filters__match_test_case__no_filters);
    ATF_ADD_TEST_CASE(tcs, test_filters__match_test_case__some_filters);
    ATF_ADD_TEST_CASE(tcs, test_filters__match_test_program__no_filters);
    ATF_ADD_TEST_CASE(tcs, test_filters__match_test_program__some_filters);
}
