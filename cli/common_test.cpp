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
#include "engine/filters.hpp"
#include "engine/test_case.hpp"
#include "engine/user_files/kyuafile.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/globals.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui_mock.hpp"
#include "utils/fs/path.hpp"
#include "utils/test_utils.hpp"

namespace cmdline = utils::cmdline;
namespace fs = utils::fs;
namespace user_files = engine::user_files;


namespace {


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


/// Syntactic sugar to instantiate engine::test_filter objects.
inline engine::test_filter
mkfilter(const char* test_program, const char* test_case)
{
    return engine::test_filter(fs::path(test_program), test_case);
}


/// Ensures that a loaded configuration was created with create_mock_kyuafile().
///
/// \param config The configuration to validate.
/// \param cookie The magic value to expect in the configuration file.
static void
validate_mock_kyuafile(const user_files::kyuafile& kyuafile, const char* cookie)
{
    const engine::test_programs_vector& test_programs =
        kyuafile.test_programs();
    ATF_REQUIRE_EQ(1, test_programs.size());
    ATF_REQUIRE_EQ(cookie, test_programs[0]->relative_path().str());
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(load_kyuafile__default);
ATF_TEST_CASE_BODY(load_kyuafile__default)
{
    std::map< std::string, std::vector< std::string > > options;
    options["kyuafile"].push_back(cli::kyuafile_option.default_value());
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    create_mock_kyuafile("Kyuafile", "foo bar");
    const user_files::kyuafile config = cli::load_kyuafile(mock_cmdline);
    validate_mock_kyuafile(config, "foo bar");
}


ATF_TEST_CASE_WITHOUT_HEAD(load_kyuafile__explicit);
ATF_TEST_CASE_BODY(load_kyuafile__explicit)
{
    std::map< std::string, std::vector< std::string > > options;
    options["kyuafile"].push_back("another");
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    create_mock_kyuafile("Kyuafile", "no no no");
    create_mock_kyuafile("another", "yes yes yes");
    const user_files::kyuafile config = cli::load_kyuafile(mock_cmdline);
    validate_mock_kyuafile(config, "yes yes yes");
}


ATF_TEST_CASE_WITHOUT_HEAD(load_kyuafile__fail);
ATF_TEST_CASE_BODY(load_kyuafile__fail)
{
    std::map< std::string, std::vector< std::string > > options;
    options["kyuafile"].push_back("missing-file");
    const cmdline::parsed_cmdline mock_cmdline(options, cmdline::args_vector());

    create_mock_kyuafile("Kyuafile", "no no no");
    ATF_REQUIRE_THROW_RE(engine::error, "missing-file",
                         cli::load_kyuafile(mock_cmdline));
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


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, load_kyuafile__default);
    ATF_ADD_TEST_CASE(tcs, load_kyuafile__explicit);
    ATF_ADD_TEST_CASE(tcs, load_kyuafile__fail);

    ATF_ADD_TEST_CASE(tcs, parse_filters__none);
    ATF_ADD_TEST_CASE(tcs, parse_filters__ok);
    ATF_ADD_TEST_CASE(tcs, parse_filters__duplicate);
    ATF_ADD_TEST_CASE(tcs, parse_filters__nondisjoint);
    ATF_ADD_TEST_CASE(tcs, report_unused_filters__none);
    ATF_ADD_TEST_CASE(tcs, report_unused_filters__some);
}
