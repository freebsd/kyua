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

#include <cstdlib>

#include <atf-c++.hpp>

#include "cli/cmd_help.hpp"
#include "utils/cmdline/base_command.ipp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/globals.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/cmdline/ui_mock.hpp"
#include "utils/sanity.hpp"
#include "utils/test_utils.hpp"

namespace cmdline = utils::cmdline;

using cli::cmd_help;


namespace {


class cmd_mock_simple : public cmdline::base_command {
public:
    cmd_mock_simple(void) : cmdline::base_command(
        "mock_simple", "", 0, 0, "Simple command")
    {
    }

    int
    run(utils::cmdline::ui* ui, const utils::cmdline::parsed_cmdline& cmdline) {
        UNREACHABLE;
        return 1234;
    }
};


class cmd_mock_complex : public cmdline::base_command {
public:
    cmd_mock_complex(void) : cmdline::base_command(
        "mock_complex", "[arg1 .. argN]", 0, 2, "Complex command")
    {
        add_option(cmdline::bool_option("flag_a", "Flag A"));
        add_option(cmdline::bool_option('b', "flag_b", "Flag B"));
        add_option(cmdline::string_option('c', "flag_c", "Flag C", "c_arg"));
        add_option(cmdline::string_option("flag_d", "Flag D", "d_arg", "foo"));
    }

    int
    run(utils::cmdline::ui* ui, const utils::cmdline::parsed_cmdline& cmdline) {
        UNREACHABLE;
        return 5678;
    }
};


static void
setup(cmdline::commands_map& commands)
{
    cmdline::init("progname");

    commands.insert(cmdline::command_ptr(new cmd_mock_simple()));
    commands.insert(cmdline::command_ptr(new cmd_mock_complex()));
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(global);
ATF_TEST_CASE_BODY(global)
{
    cmdline::commands_map mock_commands;
    setup(mock_commands);

    cmdline::args_vector args;
    args.push_back("help");

    cmd_help cmd(&mock_commands);
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args));
    ATF_REQUIRE(utils::grep_vector("^Usage: progname \\[general_options\\] "
                                   "command \\[options\\] \\[args\\]$",
                                   ui.out_log()));
    ATF_REQUIRE(utils::grep_vector("mock_simple.*Simple command",
                                   ui.out_log()));
    ATF_REQUIRE(utils::grep_vector("mock_complex.*Complex command",
                                   ui.out_log()));
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(subcommand__simple);
ATF_TEST_CASE_BODY(subcommand__simple)
{
    cmdline::commands_map mock_commands;
    setup(mock_commands);

    cmdline::args_vector args;
    args.push_back("help");
    args.push_back("mock_simple");

    cmd_help cmd(&mock_commands);
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args));
    ATF_REQUIRE(utils::grep_vector("^Usage: progname \\[general_options\\] "
                                   "mock_simple$", ui.out_log()));
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(subcommand__complex);
ATF_TEST_CASE_BODY(subcommand__complex)
{
    cmdline::commands_map mock_commands;
    setup(mock_commands);

    cmdline::args_vector args;
    args.push_back("help");
    args.push_back("mock_complex");

    cmd_help cmd(&mock_commands);
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args));
    ATF_REQUIRE(utils::grep_vector("^Usage: progname \\[general_options\\] "
                                   "mock_complex \\[options\\] "
                                   "\\[arg1 .. argN\\]$", ui.out_log()));
    ATF_REQUIRE(utils::grep_vector("--flag_a:.*Flag A", ui.out_log()));
    ATF_REQUIRE(utils::grep_vector("-b.*--flag_b:.*Flag B", ui.out_log()));
    ATF_REQUIRE(utils::grep_vector("-c c_arg.*--flag_c=c_arg:.*Flag C",
                                   ui.out_log()));
    ATF_REQUIRE(utils::grep_vector("--flag_d=d_arg:.*Flag D.*default.*foo",
                                   ui.out_log()));
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(subcommand__unknown);
ATF_TEST_CASE_BODY(subcommand__unknown)
{
    cmdline::commands_map mock_commands;
    setup(mock_commands);

    cmdline::args_vector args;
    args.push_back("help");
    args.push_back("foobar");

    cmd_help cmd(&mock_commands);
    cmdline::ui_mock ui;
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "command foobar.*not exist",
                         cmd.main(&ui, args));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(invalid_args);
ATF_TEST_CASE_BODY(invalid_args)
{
    cmdline::commands_map mock_commands;
    setup(mock_commands);

    cmdline::args_vector args;
    args.push_back("help");
    args.push_back("mock_simple");
    args.push_back("mock_complex");

    cmd_help cmd(&mock_commands);
    cmdline::ui_mock ui;
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "Too many arguments",
                         cmd.main(&ui, args));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, global);
    ATF_ADD_TEST_CASE(tcs, subcommand__simple);
    ATF_ADD_TEST_CASE(tcs, subcommand__complex);
    ATF_ADD_TEST_CASE(tcs, subcommand__unknown);
    ATF_ADD_TEST_CASE(tcs, invalid_args);
}
