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

#include "cli/common.hpp"

#include <fstream>

#include <atf-c++.hpp>

#include "engine/exceptions.hpp"
#include "engine/filters.hpp"
#include "engine/test_case.hpp"
#include "engine/test_program.hpp"
#include "engine/test_result.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/globals.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui_mock.hpp"
#include "utils/env.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"
#include "utils/test_utils.hpp"

namespace cmdline = utils::cmdline;
namespace fs = utils::fs;
namespace user_files = engine::user_files;

using utils::optional;


namespace {


/// Syntactic sugar to instantiate engine::test_filter objects.
inline engine::test_filter
mkfilter(const char* test_program, const char* test_case)
{
    return engine::test_filter(fs::path(test_program), test_case);
}


/// Fake implementation of a test program.
class mock_test_program : public engine::base_test_program {
public:
    /// Constructs a new test program.
    ///
    /// \param binary_ The name of the test program binary.
    mock_test_program(const fs::path& binary_) :
        base_test_program(binary_, fs::path("unused-root"), "unused-suite-name")
    {
    }

    /// Gets the list of test cases from the test program.
    ///
    /// \return Nothing; this method is not supposed to be called.
    engine::test_cases_vector
    load_test_cases(void) const
    {
        UNREACHABLE;
    }
};


/// Fake implementation of a test case.
class mock_test_case : public engine::base_test_case {
    /// Gets the collection of metadata properties of the test case.
    ///
    /// \return A static collection of properties for testing purposes.
    engine::properties_map
    get_all_properties(void) const
    {
        return engine::properties_map();
    }

    /// Fakes the execution of a test case.
    ///
    /// \param unused_config The user configuration that defines the execution
    ///     of this test case.
    /// \param unused_hooks Hooks to introspect the execution of the test case.
    /// \param unused_stdout_path The file to which to redirect the stdout of
    ///     the test.
    /// \param unused_stderr_path The file to which to redirect the stderr of
    ///     the test.
    ///
    /// \return Nothing; this method is not supposed to be called.
    engine::test_result
    execute(const user_files::config& UTILS_UNUSED_PARAM(config),
            engine::test_case_hooks& UTILS_UNUSED_PARAM(hooks),
            const optional< fs::path >& UTILS_UNUSED_PARAM(stdout_path),
            const optional< fs::path >& UTILS_UNUSED_PARAM(stderr_path)) const
    {
        UNREACHABLE;
    }

public:
    /// Constructs a new test case.
    ///
    /// \param test_program_ The test program this test case belongs to.
    /// \param name_ The name of the test case within the test program.
    mock_test_case(const engine::base_test_program& test_program_,
                   const std::string& name_) :
        base_test_case(test_program_, name_)
    {
    }
};


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(get_home__ok);
ATF_TEST_CASE_BODY(get_home__ok)
{
    const fs::path home("/foo/bar");
    utils::setenv("HOME", home.str());
    const optional< fs::path > computed = cli::get_home();
    ATF_REQUIRE(computed);
    ATF_REQUIRE_EQ(home, computed.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(get_home__missing);
ATF_TEST_CASE_BODY(get_home__missing)
{
    utils::unsetenv("HOME");
    ATF_REQUIRE(!cli::get_home());
}


ATF_TEST_CASE_WITHOUT_HEAD(get_home__invalid);
ATF_TEST_CASE_BODY(get_home__invalid)
{
    utils::setenv("HOME", "");
    ATF_REQUIRE(!cli::get_home());
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile_path__default);
ATF_TEST_CASE_BODY(kyuafile_path__default)
{
    std::map< std::string, std::vector< std::string > > options;
    options["kyuafile"].push_back(cli::kyuafile_option.default_value());
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    ATF_REQUIRE_EQ(cli::kyuafile_option.default_value(),
                   cli::kyuafile_path(mock_cmdline).str());
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile_path__explicit);
ATF_TEST_CASE_BODY(kyuafile_path__explicit)
{
    std::map< std::string, std::vector< std::string > > options;
    options["kyuafile"].push_back("/my//path");
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    ATF_REQUIRE_EQ("/my/path", cli::kyuafile_path(mock_cmdline).str());
}


ATF_TEST_CASE_WITHOUT_HEAD(store_path__default__create_directory__ok);
ATF_TEST_CASE_BODY(store_path__default__create_directory__ok)
{
    std::map< std::string, std::vector< std::string > > options;
    options["store"].push_back(cli::store_option.default_value());
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    const fs::path home("homedir");
    utils::setenv("HOME", home.str());

    ATF_REQUIRE(!fs::exists(home / ".kyua"));
    ATF_REQUIRE_EQ(home / ".kyua/store.db", cli::store_path(mock_cmdline));
    ATF_REQUIRE(fs::exists(home / ".kyua"));
}


ATF_TEST_CASE(store_path__default__create_directory__fail);
ATF_TEST_CASE_HEAD(store_path__default__create_directory__fail)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(store_path__default__create_directory__fail)
{
    std::map< std::string, std::vector< std::string > > options;
    options["store"].push_back(cli::store_option.default_value());
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    const fs::path home("homedir");
    utils::setenv("HOME", home.str());
    fs::mkdir(home, 0555);

    ATF_REQUIRE_THROW(fs::error, cli::store_path(mock_cmdline).str());
    ATF_REQUIRE(!fs::exists(home / ".kyua"));
}


ATF_TEST_CASE_WITHOUT_HEAD(store_path__default__no_home);
ATF_TEST_CASE_BODY(store_path__default__no_home)
{
    std::map< std::string, std::vector< std::string > > options;
    options["store"].push_back(cli::store_option.default_value());
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    utils::unsetenv("HOME");

    ATF_REQUIRE_EQ("kyua-store.db", cli::store_path(mock_cmdline).str());
}


ATF_TEST_CASE_WITHOUT_HEAD(store_path__explicit);
ATF_TEST_CASE_BODY(store_path__explicit)
{
    std::map< std::string, std::vector< std::string > > options;
    options["store"].push_back("/my//path");
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    const fs::path home("homedir");
    ATF_REQUIRE_EQ("/my/path", cli::store_path(mock_cmdline).str());
    ATF_REQUIRE(!fs::exists(home / ".kyua"));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_filters__none);
ATF_TEST_CASE_BODY(parse_filters__none)
{
    const cmdline::args_vector args;
    const std::set< engine::test_filter > filters = cli::parse_filters(args);
    ATF_REQUIRE(filters.empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_filters__ok);
ATF_TEST_CASE_BODY(parse_filters__ok)
{
    cmdline::args_vector args;
    args.push_back("foo");
    args.push_back("bar/baz");
    args.push_back("other:abc");
    args.push_back("other:bcd");
    const std::set< engine::test_filter > filters = cli::parse_filters(args);

    std::set< engine::test_filter > exp_filters;
    exp_filters.insert(mkfilter("foo", ""));
    exp_filters.insert(mkfilter("bar/baz", ""));
    exp_filters.insert(mkfilter("other", "abc"));
    exp_filters.insert(mkfilter("other", "bcd"));

    ATF_REQUIRE(exp_filters == filters);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_filters__duplicate);
ATF_TEST_CASE_BODY(parse_filters__duplicate)
{
    cmdline::args_vector args;
    args.push_back("foo/bar//baz");
    args.push_back("hello/world:yes");
    args.push_back("foo//bar/baz");
    ATF_REQUIRE_THROW_RE(cmdline::error, "Duplicate.*'foo/bar/baz'",
                         cli::parse_filters(args));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_filters__nondisjoint);
ATF_TEST_CASE_BODY(parse_filters__nondisjoint)
{
    cmdline::args_vector args;
    args.push_back("foo/bar");
    args.push_back("hello/world:yes");
    args.push_back("foo/bar:baz");
    ATF_REQUIRE_THROW_RE(cmdline::error, "'foo/bar'.*'foo/bar:baz'.*disjoint",
                         cli::parse_filters(args));
}


ATF_TEST_CASE_WITHOUT_HEAD(report_unused_filters__none);
ATF_TEST_CASE_BODY(report_unused_filters__none)
{
    std::set< engine::test_filter > unused;

    cmdline::ui_mock ui;
    ATF_REQUIRE(!cli::report_unused_filters(unused, &ui));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(report_unused_filters__some);
ATF_TEST_CASE_BODY(report_unused_filters__some)
{
    std::set< engine::test_filter > unused;
    unused.insert(mkfilter("a/b", ""));
    unused.insert(mkfilter("hey/d", "yes"));

    cmdline::ui_mock ui;
    cmdline::init("progname");
    ATF_REQUIRE(cli::report_unused_filters(unused, &ui));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE_EQ(2, ui.err_log().size());
    ATF_REQUIRE( utils::grep_vector("No.*matched.*'a/b'", ui.err_log()));
    ATF_REQUIRE( utils::grep_vector("No.*matched.*'hey/d:yes'", ui.err_log()));
}


ATF_TEST_CASE_WITHOUT_HEAD(format_result__no_reason);
ATF_TEST_CASE_BODY(format_result__no_reason)
{
    ATF_REQUIRE_EQ("passed", cli::format_result(
        engine::test_result(engine::test_result::passed)));
    ATF_REQUIRE_EQ("failed", cli::format_result(
        engine::test_result(engine::test_result::failed)));
}


ATF_TEST_CASE_WITHOUT_HEAD(format_result__with_reason);
ATF_TEST_CASE_BODY(format_result__with_reason)
{
    ATF_REQUIRE_EQ("broken: Something", cli::format_result(
        engine::test_result(engine::test_result::broken, "Something")));
    ATF_REQUIRE_EQ("expected_failure: A B C", cli::format_result(
        engine::test_result(engine::test_result::expected_failure, "A B C")));
    ATF_REQUIRE_EQ("failed: More text", cli::format_result(
        engine::test_result(engine::test_result::failed, "More text")));
    ATF_REQUIRE_EQ("skipped: Bye", cli::format_result(
        engine::test_result(engine::test_result::skipped, "Bye")));
}


ATF_TEST_CASE_WITHOUT_HEAD(format_test_case_id__test_case);
ATF_TEST_CASE_BODY(format_test_case_id__test_case)
{
    const mock_test_program test_program(fs::path("foo/bar/baz"));
    const mock_test_case test_case(test_program, "abc");
    ATF_REQUIRE_EQ("foo/bar/baz:abc", cli::format_test_case_id(test_case));
}


ATF_TEST_CASE_WITHOUT_HEAD(format_test_case_id__test_filter);
ATF_TEST_CASE_BODY(format_test_case_id__test_filter)
{
    const engine::test_filter filter(fs::path("foo/bar"), "baz");
    ATF_REQUIRE_EQ("foo/bar:baz", cli::format_test_case_id(filter));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, get_home__ok);
    ATF_ADD_TEST_CASE(tcs, get_home__missing);
    ATF_ADD_TEST_CASE(tcs, get_home__invalid);

    ATF_ADD_TEST_CASE(tcs, kyuafile_path__default);
    ATF_ADD_TEST_CASE(tcs, kyuafile_path__explicit);

    ATF_ADD_TEST_CASE(tcs, store_path__default__create_directory__ok);
    ATF_ADD_TEST_CASE(tcs, store_path__default__create_directory__fail);
    ATF_ADD_TEST_CASE(tcs, store_path__default__no_home);
    ATF_ADD_TEST_CASE(tcs, store_path__explicit);

    ATF_ADD_TEST_CASE(tcs, parse_filters__none);
    ATF_ADD_TEST_CASE(tcs, parse_filters__ok);
    ATF_ADD_TEST_CASE(tcs, parse_filters__duplicate);
    ATF_ADD_TEST_CASE(tcs, parse_filters__nondisjoint);

    ATF_ADD_TEST_CASE(tcs, report_unused_filters__none);
    ATF_ADD_TEST_CASE(tcs, report_unused_filters__some);

    ATF_ADD_TEST_CASE(tcs, format_result__no_reason);
    ATF_ADD_TEST_CASE(tcs, format_result__with_reason);

    ATF_ADD_TEST_CASE(tcs, format_test_case_id__test_case);
    ATF_ADD_TEST_CASE(tcs, format_test_case_id__test_filter);
}
