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

extern "C" {
#include <signal.h>
}

#include <cstdlib>

#include <atf-c++.hpp>

#include "cli/main.hpp"
#include "utils/cmdline/base_command.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/globals.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/cmdline/ui_mock.hpp"
#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/process/children.ipp"
#include "utils/process/status.hpp"
#include "utils/test_utils.hpp"

namespace cmdline = utils::cmdline;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace process = utils::process;


namespace {


class cmd_mock_crash : public cmdline::base_command {
    bool _unhandled;

public:
    cmd_mock_crash(void) :
        cmdline::base_command("mock_error", "", 0, 0,
                              "Mock command that crashes")
    {
    }

    int
    run(utils::cmdline::ui* ui, const utils::cmdline::parsed_cmdline& cmdline)
    {
        std::abort();
    }
};


class cmd_mock_error : public cmdline::base_command {
    bool _unhandled;

public:
    cmd_mock_error(const bool unhandled) :
        cmdline::base_command("mock_error", "", 0, 0,
                              "Mock command that raises an error"),
        _unhandled(unhandled)
    {
    }

    int
    run(utils::cmdline::ui* ui, const utils::cmdline::parsed_cmdline& cmdline)
    {
        if (_unhandled)
            throw std::logic_error("This is unhandled");
        else
            throw std::runtime_error("Runtime error");
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


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(detail__default_log_name__home);
ATF_TEST_CASE_BODY(detail__default_log_name__home)
{
    datetime::set_mock_now(2011, 2, 21, 21, 10, 30);
    cmdline::init("progname1");

    utils::setenv("HOME", "/home//fake");
    utils::setenv("TMPDIR", "/do/not/use/this");
    ATF_REQUIRE_EQ(
        fs::path("/home/fake/.kyua/logs/progname1.20110221-211030.log"),
        cli::detail::default_log_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(detail__default_log_name__tmpdir);
ATF_TEST_CASE_BODY(detail__default_log_name__tmpdir)
{
    datetime::set_mock_now(2011, 2, 21, 21, 10, 50);
    cmdline::init("progname2");

    utils::unsetenv("HOME");
    utils::setenv("TMPDIR", "/a/b//c");
    ATF_REQUIRE_EQ(fs::path("/a/b/c/progname2.20110221-211050.log"),
                   cli::detail::default_log_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(detail__default_log_name__hardcoded);
ATF_TEST_CASE_BODY(detail__default_log_name__hardcoded)
{
    datetime::set_mock_now(2011, 2, 21, 21, 15, 00);
    cmdline::init("progname3");

    utils::unsetenv("HOME");
    utils::unsetenv("TMPDIR");
    ATF_REQUIRE_EQ(fs::path("/tmp/progname3.20110221-211500.log"),
                   cli::detail::default_log_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(main__no_args);
ATF_TEST_CASE_BODY(main__no_args)
{
    cmdline::init("progname");

    const int argc = 1;
    const char* const argv[] = {"progname", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_FAILURE, cli::main(&ui, argc, argv));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(utils::grep_vector("Usage error: No command provided",
                                   ui.err_log()));
    ATF_REQUIRE(utils::grep_vector("Type.*progname help", ui.err_log()));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__unknown_command);
ATF_TEST_CASE_BODY(main__unknown_command)
{
    cmdline::init("progname");

    const int argc = 2;
    const char* const argv[] = {"progname", "foo", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_FAILURE, cli::main(&ui, argc, argv));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(utils::grep_vector("Usage error: Unknown command.*foo",
                                   ui.err_log()));
    ATF_REQUIRE(utils::grep_vector("Type.*progname help", ui.err_log()));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__logfile__default);
ATF_TEST_CASE_BODY(main__logfile__default)
{
    datetime::set_mock_now(2011, 2, 21, 21, 30, 00);
    cmdline::init("progname");

    const int argc = 1;
    const char* const argv[] = {"progname", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE(!fs::exists(fs::path(
        ".kyua/logs/progname.20110221-213000.log")));
    ATF_REQUIRE_EQ(EXIT_FAILURE, cli::main(&ui, argc, argv));
    ATF_REQUIRE(fs::exists(fs::path(
        ".kyua/logs/progname.20110221-213000.log")));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__logfile__override);
ATF_TEST_CASE_BODY(main__logfile__override)
{
    datetime::set_mock_now(2011, 2, 21, 21, 30, 00);
    cmdline::init("progname");

    const int argc = 2;
    const char* const argv[] = {"progname", "--logfile=test.log", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE(!fs::exists(fs::path("test.log")));
    ATF_REQUIRE_EQ(EXIT_FAILURE, cli::main(&ui, argc, argv));
    ATF_REQUIRE(!fs::exists(fs::path(
        ".kyua/logs/progname.20110221-213000.log")));
    ATF_REQUIRE(fs::exists(fs::path("test.log")));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__loglevel__default);
ATF_TEST_CASE_BODY(main__loglevel__default)
{
    cmdline::init("progname");

    const int argc = 2;
    const char* const argv[] = {"progname", "--logfile=test.log", NULL};

    LD("Mock debug message");
    LE("Mock error message");
    LI("Mock info message");
    LW("Mock warning message");

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_FAILURE, cli::main(&ui, argc, argv));
    ATF_REQUIRE(!utils::grep_file("Mock debug message", fs::path("test.log")));
    ATF_REQUIRE(utils::grep_file("Mock error message", fs::path("test.log")));
    ATF_REQUIRE(utils::grep_file("Mock info message", fs::path("test.log")));
    ATF_REQUIRE(utils::grep_file("Mock warning message", fs::path("test.log")));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__loglevel__higher);
ATF_TEST_CASE_BODY(main__loglevel__higher)
{
    cmdline::init("progname");

    const int argc = 3;
    const char* const argv[] = {"progname", "--logfile=test.log",
                                "--loglevel=debug", NULL};

    LD("Mock debug message");
    LE("Mock error message");
    LI("Mock info message");
    LW("Mock warning message");

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_FAILURE, cli::main(&ui, argc, argv));
    ATF_REQUIRE(utils::grep_file("Mock debug message", fs::path("test.log")));
    ATF_REQUIRE(utils::grep_file("Mock error message", fs::path("test.log")));
    ATF_REQUIRE(utils::grep_file("Mock info message", fs::path("test.log")));
    ATF_REQUIRE(utils::grep_file("Mock warning message", fs::path("test.log")));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__loglevel__lower);
ATF_TEST_CASE_BODY(main__loglevel__lower)
{
    cmdline::init("progname");

    const int argc = 3;
    const char* const argv[] = {"progname", "--logfile=test.log",
                                "--loglevel=warning", NULL};

    LD("Mock debug message");
    LE("Mock error message");
    LI("Mock info message");
    LW("Mock warning message");

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_FAILURE, cli::main(&ui, argc, argv));
    ATF_REQUIRE(!utils::grep_file("Mock debug message", fs::path("test.log")));
    ATF_REQUIRE(utils::grep_file("Mock error message", fs::path("test.log")));
    ATF_REQUIRE(!utils::grep_file("Mock info message", fs::path("test.log")));
    ATF_REQUIRE(utils::grep_file("Mock warning message", fs::path("test.log")));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__loglevel__error);
ATF_TEST_CASE_BODY(main__loglevel__error)
{
    cmdline::init("progname");

    const int argc = 3;
    const char* const argv[] = {"progname", "--logfile=test.log",
                                "--loglevel=i-am-invalid", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_FAILURE, cli::main(&ui, argc, argv));
    ATF_REQUIRE(utils::grep_vector("Usage error.*i-am-invalid", ui.err_log()));
    ATF_REQUIRE(!fs::exists(fs::path("test.log")));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__subcommand__ok);
ATF_TEST_CASE_BODY(main__subcommand__ok)
{
    cmdline::init("progname");

    const int argc = 2;
    const char* const argv[] = {"progname", "mock_write", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(98, cli::main(&ui, argc, argv,
                                 cmdline::command_ptr(new cmd_mock_write())));
    ATF_REQUIRE_EQ(1, ui.out_log().size());
    ATF_REQUIRE_EQ("stdout message from subcommand", ui.out_log()[0]);
    ATF_REQUIRE_EQ(1, ui.err_log().size());
    ATF_REQUIRE_EQ("stderr message from subcommand", ui.err_log()[0]);
}


ATF_TEST_CASE_WITHOUT_HEAD(main__subcommand__invalid_args);
ATF_TEST_CASE_BODY(main__subcommand__invalid_args)
{
    cmdline::init("progname");

    const int argc = 3;
    const char* const argv[] = {"progname", "mock_write", "bar", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_FAILURE,
                   cli::main(&ui, argc, argv,
                             cmdline::command_ptr(new cmd_mock_write())));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(utils::grep_vector(
        "Usage error for command mock_write: Too many arguments.",
        ui.err_log()));
    ATF_REQUIRE(utils::grep_vector("Type.*progname help", ui.err_log()));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__subcommand__runtime_error);
ATF_TEST_CASE_BODY(main__subcommand__runtime_error)
{
    cmdline::init("progname");

    const int argc = 2;
    const char* const argv[] = {"progname", "mock_error", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_FAILURE, cli::main(&ui, argc, argv,
        cmdline::command_ptr(new cmd_mock_error(false))));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(utils::grep_vector("progname: E: Runtime error.",
                                   ui.err_log()));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__subcommand__unhandled_exception);
ATF_TEST_CASE_BODY(main__subcommand__unhandled_exception)
{
    cmdline::init("progname");

    const int argc = 2;
    const char* const argv[] = {"progname", "mock_error", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE_THROW(std::logic_error, cli::main(&ui, argc, argv,
        cmdline::command_ptr(new cmd_mock_error(true))));
}


static void
do_subcommand_crash(void)
{
    cmdline::init("progname");

    const int argc = 2;
    const char* const argv[] = {"progname", "mock_error", NULL};

    cmdline::ui_mock ui;
    cli::main(&ui, argc, argv,
              cmdline::command_ptr(new cmd_mock_crash()));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__subcommand__crash);
ATF_TEST_CASE_BODY(main__subcommand__crash)
{
    const process::status status = process::child_with_files::fork(
        do_subcommand_crash, fs::path("stdout.txt"),
        fs::path("stderr.txt"))->wait();
    ATF_REQUIRE(status.signaled());
    ATF_REQUIRE_EQ(SIGABRT, status.termsig());
    ATF_REQUIRE(utils::grep_file("Fatal signal", fs::path("stderr.txt")));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, detail__default_log_name__home);
    ATF_ADD_TEST_CASE(tcs, detail__default_log_name__tmpdir);
    ATF_ADD_TEST_CASE(tcs, detail__default_log_name__hardcoded);

    ATF_ADD_TEST_CASE(tcs, main__no_args);
    ATF_ADD_TEST_CASE(tcs, main__unknown_command);
    ATF_ADD_TEST_CASE(tcs, main__logfile__default);
    ATF_ADD_TEST_CASE(tcs, main__logfile__override);
    ATF_ADD_TEST_CASE(tcs, main__loglevel__default);
    ATF_ADD_TEST_CASE(tcs, main__loglevel__higher);
    ATF_ADD_TEST_CASE(tcs, main__loglevel__lower);
    ATF_ADD_TEST_CASE(tcs, main__loglevel__error);
    ATF_ADD_TEST_CASE(tcs, main__subcommand__ok);
    ATF_ADD_TEST_CASE(tcs, main__subcommand__invalid_args);
    ATF_ADD_TEST_CASE(tcs, main__subcommand__runtime_error);
    ATF_ADD_TEST_CASE(tcs, main__subcommand__unhandled_exception);
    ATF_ADD_TEST_CASE(tcs, main__subcommand__crash);
}
