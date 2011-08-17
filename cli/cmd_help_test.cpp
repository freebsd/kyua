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
#include "cli/common.ipp"
#include "engine/user_files/config.hpp"
#include "utils/cmdline/commands_map.ipp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/globals.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/cmdline/ui_mock.hpp"
#include "utils/defs.hpp"
#include "utils/sanity.hpp"
#include "utils/test_utils.hpp"

namespace cmdline = utils::cmdline;
namespace user_files = engine::user_files;

using cli::cmd_help;


namespace {


static const user_files::config default_config = user_files::config::defaults();


class cmd_mock_simple : public cli::cli_command {
public:
    cmd_mock_simple(void) : cli::cli_command(
        "mock_simple", "", 0, 0, "Simple command")
    {
    }

    int
    run(cmdline::ui* UTILS_UNUSED_PARAM(ui),
        const cmdline::parsed_cmdline& UTILS_UNUSED_PARAM(cmdline),
        const user_files::config& UTILS_UNUSED_PARAM(config))
    {
        UNREACHABLE;
        return 1234;
    }
};


class cmd_mock_complex : public cli::cli_command {
public:
    cmd_mock_complex(void) : cli::cli_command(
        "mock_complex", "[arg1 .. argN]", 0, 2, "Complex command")
    {
        add_option(cmdline::bool_option("flag_a", "Flag A"));
        add_option(cmdline::bool_option('b', "flag_b", "Flag B"));
        add_option(cmdline::string_option('c', "flag_c", "Flag C", "c_arg"));
        add_option(cmdline::string_option("flag_d", "Flag D", "d_arg", "foo"));
    }

    int
    run(cmdline::ui* UTILS_UNUSED_PARAM(ui),
        const cmdline::parsed_cmdline& UTILS_UNUSED_PARAM(cmdline),
        const user_files::config& UTILS_UNUSED_PARAM(config))
    {
        UNREACHABLE;
        return 5678;
    }
};


static void
setup(cmdline::commands_map< cli::cli_command >& commands)
{
    cmdline::init("progname");

    commands.insert(new cmd_mock_simple());
    commands.insert(new cmd_mock_complex());
}


/// Performs a test on the global help (not that of a subcommand).
///
/// \param general_options The genral options supported by the tool, if any.
/// \param ui The cmdline::mock_ui object to which to write the output.
static void
global_test(const cmdline::options_vector& general_options,
            cmdline::ui_mock& ui)
{
    cmdline::commands_map< cli::cli_command > mock_commands;
    setup(mock_commands);

    cmdline::args_vector args;
    args.push_back("help");

    cmd_help cmd(&general_options, &mock_commands);
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args, default_config));
    ATF_REQUIRE(utils::grep_vector("^Usage: progname \\[general_options\\] "
                                   "command \\[command_options\\] \\[args\\]$",
                                   ui.out_log()));
    if (general_options.empty())
        ATF_REQUIRE(!utils::grep_vector("Available general options",
                                        ui.out_log()));
    else
        ATF_REQUIRE(utils::grep_vector("Available general options",
                                       ui.out_log()));
    ATF_REQUIRE(utils::grep_vector("mock_simple.*Simple command",
                                   ui.out_log()));
    ATF_REQUIRE(utils::grep_vector("mock_complex.*Complex command",
                                   ui.out_log()));
    ATF_REQUIRE(ui.err_log().empty());
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(global__no_options);
ATF_TEST_CASE_BODY(global__no_options)
{
    cmdline::ui_mock ui;

    cmdline::options_vector general_options;

    global_test(general_options, ui);
}


ATF_TEST_CASE_WITHOUT_HEAD(global__some_options);
ATF_TEST_CASE_BODY(global__some_options)
{
    cmdline::ui_mock ui;

    cmdline::options_vector general_options;
    const cmdline::bool_option flag_a("flag_a", "Flag A");
    general_options.push_back(&flag_a);
    const cmdline::string_option flag_c('c', "flag_c", "Flag C", "c_arg");
    general_options.push_back(&flag_c);

    global_test(general_options, ui);

    ATF_REQUIRE(utils::grep_vector("--flag_a", ui.out_log()));
    ATF_REQUIRE(utils::grep_vector("--flag_c=c_arg", ui.out_log()));
}


ATF_TEST_CASE_WITHOUT_HEAD(subcommand__simple);
ATF_TEST_CASE_BODY(subcommand__simple)
{
    cmdline::options_vector general_options;

    cmdline::commands_map< cli::cli_command > mock_commands;
    setup(mock_commands);

    cmdline::args_vector args;
    args.push_back("help");
    args.push_back("mock_simple");

    cmd_help cmd(&general_options, &mock_commands);
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args, default_config));
    ATF_REQUIRE(utils::grep_vector("^Usage: progname \\[general_options\\] "
                                   "mock_simple$", ui.out_log()));
    ATF_REQUIRE(!utils::grep_vector("Available.*options", ui.out_log()));
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(subcommand__complex);
ATF_TEST_CASE_BODY(subcommand__complex)
{
    cmdline::options_vector general_options;
    const cmdline::bool_option global_a("global_a", "Global A");
    general_options.push_back(&global_a);
    const cmdline::string_option global_c('c', "global_c", "Global C",
                                          "c_global");
    general_options.push_back(&global_c);

    cmdline::commands_map< cli::cli_command > mock_commands;
    setup(mock_commands);

    cmdline::args_vector args;
    args.push_back("help");
    args.push_back("mock_complex");

    cmd_help cmd(&general_options, &mock_commands);
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args, default_config));
    ATF_REQUIRE(utils::grep_vector("^Usage: progname \\[general_options\\] "
                                   "mock_complex \\[command_options\\] "
                                   "\\[arg1 .. argN\\]$", ui.out_log()));
    ATF_REQUIRE(utils::grep_vector("Available general options", ui.out_log()));
    ATF_REQUIRE(utils::grep_vector("--global_a", ui.out_log()));
    ATF_REQUIRE(utils::grep_vector("--global_c=c_global", ui.out_log()));
    ATF_REQUIRE(utils::grep_vector("Available command options", ui.out_log()));
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
    cmdline::options_vector general_options;

    cmdline::commands_map< cli::cli_command > mock_commands;
    setup(mock_commands);

    cmdline::args_vector args;
    args.push_back("help");
    args.push_back("foobar");

    cmd_help cmd(&general_options, &mock_commands);
    cmdline::ui_mock ui;
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "command foobar.*not exist",
                         cmd.main(&ui, args, default_config));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(invalid_args);
ATF_TEST_CASE_BODY(invalid_args)
{
    cmdline::options_vector general_options;

    cmdline::commands_map< cli::cli_command > mock_commands;
    setup(mock_commands);

    cmdline::args_vector args;
    args.push_back("help");
    args.push_back("mock_simple");
    args.push_back("mock_complex");

    cmd_help cmd(&general_options, &mock_commands);
    cmdline::ui_mock ui;
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "Too many arguments",
                         cmd.main(&ui, args, default_config));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, global__no_options);
    ATF_ADD_TEST_CASE(tcs, global__some_options);
    ATF_ADD_TEST_CASE(tcs, subcommand__simple);
    ATF_ADD_TEST_CASE(tcs, subcommand__complex);
    ATF_ADD_TEST_CASE(tcs, subcommand__unknown);
    ATF_ADD_TEST_CASE(tcs, invalid_args);
}
