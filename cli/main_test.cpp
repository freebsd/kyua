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

#include "cli/main.hpp"
#include "utils/cmdline/base_command.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/globals.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/cmdline/ui_mock.hpp"
#include "utils/test_utils.hpp"

namespace cmdline = utils::cmdline;


namespace {


class cmd_mock_error : public cmdline::base_command {
public:
    cmd_mock_error(void) : cmdline::base_command(
        "mock_error", "", 0, 0, "Mock command that raises an error")
    {
    }

    int
    run(utils::cmdline::ui* ui, const utils::cmdline::parsed_cmdline& cmdline)
    {
        throw std::runtime_error("This is unhandled");
    }
};


class cmd_mock_write : public cmdline::base_command {
public:
    cmd_mock_write(void) : cmdline::base_command(
        "mock_write", "", 0, 0, "Mock command that prints output")
    {
    }

    int
    run(utils::cmdline::ui* ui, const utils::cmdline::parsed_cmdline& cmdline)
    {
        ui->out("stdout message from subcommand");
        ui->err("stderr message from subcommand");
        return 98;
    }
};


static void
setup(cmdline::commands_map& commands)
{
    cmdline::init("progname");

    commands.insert(cmdline::command_ptr(new cmd_mock_error()));
    commands.insert(cmdline::command_ptr(new cmd_mock_write()));
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(no_args);
ATF_TEST_CASE_BODY(no_args)
{
    cmdline::commands_map mock_commands;
    setup(mock_commands);

    const int argc = 1;
    const char* const argv[] = {"progname", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_FAILURE, cli::main(&ui, argc, argv, mock_commands));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(utils::grep_vector("Usage error: No command provided",
                                   ui.err_log()));
    ATF_REQUIRE(utils::grep_vector("Type.*progname help", ui.err_log()));
}


ATF_TEST_CASE_WITHOUT_HEAD(unknown_command);
ATF_TEST_CASE_BODY(unknown_command)
{
    cmdline::commands_map mock_commands;
    setup(mock_commands);

    const int argc = 2;
    const char* const argv[] = {"progname", "foo", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_FAILURE, cli::main(&ui, argc, argv, mock_commands));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(utils::grep_vector("Usage error: Unknown command.*foo",
                                   ui.err_log()));
    ATF_REQUIRE(utils::grep_vector("Type.*progname help", ui.err_log()));
}


ATF_TEST_CASE_WITHOUT_HEAD(subcommand__ok);
ATF_TEST_CASE_BODY(subcommand__ok)
{
    cmdline::commands_map mock_commands;
    setup(mock_commands);

    const int argc = 2;
    const char* const argv[] = {"progname", "mock_write", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(98, cli::main(&ui, argc, argv, mock_commands));
    ATF_REQUIRE_EQ(1, ui.out_log().size());
    ATF_REQUIRE_EQ("stdout message from subcommand", ui.out_log()[0]);
    ATF_REQUIRE_EQ(1, ui.err_log().size());
    ATF_REQUIRE_EQ("stderr message from subcommand", ui.err_log()[0]);
}


ATF_TEST_CASE_WITHOUT_HEAD(subcommand__invalid_args);
ATF_TEST_CASE_BODY(subcommand__invalid_args)
{
    cmdline::commands_map mock_commands;
    setup(mock_commands);

    const int argc = 3;
    const char* const argv[] = {"progname", "mock_write", "bar", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_FAILURE, cli::main(&ui, argc, argv, mock_commands));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(utils::grep_vector(
        "Usage error for command mock_write: Too many arguments.",
        ui.err_log()));
    ATF_REQUIRE(utils::grep_vector("Type.*progname help", ui.err_log()));
}


ATF_TEST_CASE_WITHOUT_HEAD(subcommand__error);
ATF_TEST_CASE_BODY(subcommand__error)
{
    cmdline::commands_map mock_commands;
    setup(mock_commands);

    const int argc = 2;
    const char* const argv[] = {"progname", "mock_error", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE_THROW_RE(std::runtime_error, "unhandled",
                         cli::main(&ui, argc, argv, mock_commands));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, no_args);
    ATF_ADD_TEST_CASE(tcs, unknown_command);
    ATF_ADD_TEST_CASE(tcs, subcommand__ok);
    ATF_ADD_TEST_CASE(tcs, subcommand__invalid_args);
    ATF_ADD_TEST_CASE(tcs, subcommand__error);
}
