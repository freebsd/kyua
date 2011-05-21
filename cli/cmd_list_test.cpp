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

extern "C" {
#include <sys/stat.h>

#include <unistd.h>
}

#include <atf-c++.hpp>

#include "cli/cmd_list.hpp"
#include "cli/common.hpp"
#include "engine/exceptions.hpp"
#include "engine/test_case.hpp"
#include "engine/user_files/kyuafile.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/cmdline/ui_mock.hpp"
#include "utils/env.hpp"

namespace cmdline = utils::cmdline;
namespace user_files = engine::user_files;
namespace fs = utils::fs;


namespace {


/// Gets the path to the helpers for this test program.
///
/// \param tc A pointer to the currently running test case.
///
/// \return The path to the helpers binary.
static fs::path
helpers(const atf::tests::tc* test_case)
{
    return fs::path(test_case->get_config_var("srcdir")) /
        "cmd_list_helpers";
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(list_test_case__no_verbose);
ATF_TEST_CASE_BODY(list_test_case__no_verbose)
{
    const engine::test_case_id id(fs::path("the/test-program"), "abc");
    engine::properties_map properties;
    properties["descr"] = "Unused description";
    const engine::test_case test_case = engine::test_case::from_properties(
        id, properties);

    cmdline::ui_mock ui;
    cli::detail::list_test_case(&ui, false, test_case, "unused test suite");
    ATF_REQUIRE_EQ(1, ui.out_log().size());
    ATF_REQUIRE_EQ("the/test-program:abc", ui.out_log()[0]);
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(list_test_case__verbose__no_properties);
ATF_TEST_CASE_BODY(list_test_case__verbose__no_properties)
{
    const engine::test_case_id id(fs::path("hello/world"), "my_name");
    engine::properties_map properties;
    const engine::test_case test_case = engine::test_case::from_properties(
        id, properties);

    cmdline::ui_mock ui;
    cli::detail::list_test_case(&ui, true, test_case, "the-suite");
    ATF_REQUIRE_EQ(1, ui.out_log().size());
    ATF_REQUIRE_EQ("hello/world:my_name (the-suite)", ui.out_log()[0]);
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(list_test_case__verbose__some_properties);
ATF_TEST_CASE_BODY(list_test_case__verbose__some_properties)
{
    const engine::test_case_id id(fs::path("hello/world"), "my_name");
    engine::properties_map properties;
    properties["descr"] = "Some description";
    properties["has.cleanup"] = "true";
    const engine::test_case test_case = engine::test_case::from_properties(
        id, properties);

    cmdline::ui_mock ui;
    cli::detail::list_test_case(&ui, true, test_case, "the-suite");
    ATF_REQUIRE_EQ(3, ui.out_log().size());
    ATF_REQUIRE_EQ("hello/world:my_name (the-suite)", ui.out_log()[0]);
    ATF_REQUIRE_EQ("    descr = Some description", ui.out_log()[1]);
    ATF_REQUIRE_EQ("    has.cleanup = true", ui.out_log()[2]);
    ATF_REQUIRE(ui.err_log().empty());
}


static void
run_helpers(const atf::tests::tc* tc, cmdline::ui* ui, const bool verbose,
            const char* filter = NULL)
{
    ATF_REQUIRE(::mkdir("root", 0755) != -1);
    ATF_REQUIRE(::mkdir("root/dir", 0755) != -1);
    ATF_REQUIRE(::symlink(helpers(tc).c_str(), "root/dir/program") != -1);

    const user_files::test_program test_program(fs::path("dir/program"),
                                                "suite-name");

    utils::cmdline::args_vector args;
    if (filter != NULL)
        args.push_back(filter);
    cli::filters_state filters(args);

    cli::detail::list_test_program(ui, verbose, fs::path("root"), test_program,
                                   filters);
}


ATF_TEST_CASE_WITHOUT_HEAD(list_test_program__one_test_case__no_verbose);
ATF_TEST_CASE_BODY(list_test_program__one_test_case__no_verbose)
{
    cmdline::ui_mock ui;

    utils::setenv("TESTS", "some_properties");
    run_helpers(this, &ui, false);

    ATF_REQUIRE_EQ(1, ui.out_log().size());
    ATF_REQUIRE_EQ("dir/program:some_properties", ui.out_log()[0]);
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(list_test_program__one_test_case__verbose);
ATF_TEST_CASE_BODY(list_test_program__one_test_case__verbose)
{
    cmdline::ui_mock ui;

    utils::setenv("TESTS", "some_properties");
    run_helpers(this, &ui, true);

    ATF_REQUIRE_EQ(3, ui.out_log().size());
    ATF_REQUIRE_EQ("dir/program:some_properties (suite-name)", ui.out_log()[0]);
    ATF_REQUIRE_EQ("    descr = This is a description", ui.out_log()[1]);
    ATF_REQUIRE_EQ("    require.progs = /bin/ls non-existent", ui.out_log()[2]);
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(list_test_program__many_test_cases__no_verbose);
ATF_TEST_CASE_BODY(list_test_program__many_test_cases__no_verbose)
{
    cmdline::ui_mock ui;

    utils::setenv("TESTS", "no_properties some_properties");
    run_helpers(this, &ui, false);

    ATF_REQUIRE_EQ(2, ui.out_log().size());
    ATF_REQUIRE_EQ("dir/program:no_properties", ui.out_log()[0]);
    ATF_REQUIRE_EQ("dir/program:some_properties", ui.out_log()[1]);
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(list_test_program__many_test_cases__verbose);
ATF_TEST_CASE_BODY(list_test_program__many_test_cases__verbose)
{
    cmdline::ui_mock ui;

    utils::setenv("TESTS", "no_properties some_properties");
    run_helpers(this, &ui, true);

    ATF_REQUIRE_EQ(4, ui.out_log().size());
    ATF_REQUIRE_EQ("dir/program:no_properties (suite-name)", ui.out_log()[0]);
    ATF_REQUIRE_EQ("dir/program:some_properties (suite-name)", ui.out_log()[1]);
    ATF_REQUIRE_EQ("    descr = This is a description", ui.out_log()[2]);
    ATF_REQUIRE_EQ("    require.progs = /bin/ls non-existent", ui.out_log()[3]);
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(list_test_program__filter_match);
ATF_TEST_CASE_BODY(list_test_program__filter_match)
{
    cmdline::ui_mock ui;

    utils::setenv("TESTS", "no_properties some_properties");
    run_helpers(this, &ui, false, "dir/program:some_properties");

    ATF_REQUIRE_EQ(1, ui.out_log().size());
    ATF_REQUIRE_EQ("dir/program:some_properties", ui.out_log()[0]);
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(list_test_program__crash);
ATF_TEST_CASE_BODY(list_test_program__crash)
{
    cmdline::ui_mock ui;

    utils::setenv("TESTS", "crash_list");
    ATF_REQUIRE_THROW(engine::format_error, run_helpers(this, &ui, true));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(list_test_program__missing);
ATF_TEST_CASE_BODY(list_test_program__missing)
{
    cmdline::ui_mock ui;

    const user_files::test_program test_program(fs::path("missing"),
                                                "suite-name");

    const utils::cmdline::args_vector args;
    cli::filters_state filters(args);

    ATF_REQUIRE_THROW(engine::error, cli::detail::list_test_program(
        &ui, false, fs::path("root"), test_program, filters));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, list_test_case__no_verbose);
    ATF_ADD_TEST_CASE(tcs, list_test_case__verbose__no_properties);
    ATF_ADD_TEST_CASE(tcs, list_test_case__verbose__some_properties);

    ATF_ADD_TEST_CASE(tcs, list_test_program__one_test_case__no_verbose);
    ATF_ADD_TEST_CASE(tcs, list_test_program__one_test_case__verbose);
    ATF_ADD_TEST_CASE(tcs, list_test_program__many_test_cases__no_verbose);
    ATF_ADD_TEST_CASE(tcs, list_test_program__many_test_cases__verbose);
    ATF_ADD_TEST_CASE(tcs, list_test_program__filter_match);
    ATF_ADD_TEST_CASE(tcs, list_test_program__crash);
    ATF_ADD_TEST_CASE(tcs, list_test_program__missing);

    // Tests for cmd_list::run are located in integration/cmd_list_test.
}
