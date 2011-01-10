// Copyright 2010 Google Inc.
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
#include <sys/wait.h>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
}

#include <cstdarg>
#include <cerrno>
#include <cstdlib>
#include <iostream>

#include <atf-c++.hpp>

#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/process/children.ipp"
#include "utils/process/exceptions.hpp"
#include "utils/process/system.hpp"
#include "utils/sanity.hpp"
#include "utils/signals/timer.hpp"
#include "utils/test_utils.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace process = utils::process;
namespace signals = utils::signals;


namespace {


template< int ExitStatus, char Message >
static void
child_simple_function(void)
{
    std::cout << "To stdout: " << Message << "\n";
    std::cerr << "To stderr: " << Message << "\n";
    std::exit(ExitStatus);
}


class child_simple_functor {
    int _exitstatus;
    std::string _message;

public:
    child_simple_functor(int exitstatus, const std::string& message) :
        _exitstatus(exitstatus),
        _message(message)
    {
    }

    void
    operator()(void)
    {
        std::cout << "To stdout: " << _message << "\n";
        std::cerr << "To stderr: " << _message << "\n";
        std::exit(_exitstatus);
    }
};


static void
child_printer_function(void)
{
    for (std::size_t i = 0; i < 100; i++)
        std::cout << "This is a message to stdout, sequence " << i << "\n";
    std::cout.flush();
    std::cerr << "Exiting\n";
    std::exit(EXIT_SUCCESS);
}


class child_printer_functor {
public:
    void
    operator()(void)
    {
        child_printer_function();
    }
};


template< int Microseconds >
static void
child_wait(void)
{
    ::usleep(Microseconds);
    utils::create_file(fs::path("finished"));
    std::exit(EXIT_SUCCESS);
}


template< int Microseconds >
static void
child_wait_with_subchild(void)
{
    ::setpgid(::getpid(), ::getpid());

    const int ret = ::fork();
    if (ret == -1) {
        std::abort();
    } else if (ret == 0) {
        ::usleep(Microseconds);
        utils::create_file(fs::path("subfinished"));
        std::exit(EXIT_SUCCESS);
    } else {
        ::usleep(Microseconds);
        utils::create_file(fs::path("finished"));

        int status;
        (void)::wait(&status);
        std::exit(EXIT_SUCCESS);
    }
}


class do_exec {
    fs::path _program;
    const std::vector< std::string > _args;

public:
    do_exec(const fs::path& program, const std::vector< std::string >& args) :
        _program(program),
        _args(args)
    {
    }

    void
    operator()(void)
    {
        process::exec(_program, _args);
    }
};


static fs::path
get_helpers(const atf::tests::tc* tc)
{
    return fs::path(tc->get_config_var("srcdir")) / "helpers";
}


template< int Errno >
static pid_t
fork_fail(void)
{
    errno = Errno;
    return -1;
}


template< int Errno >
static pid_t
open_fail(const char* path, const int flags, ...)
{
    if (std::strcmp(path, "raise-error") == 0) {
        errno = Errno;
        return -1;
    } else {
        va_list ap;
        va_start(ap, flags);
        const int mode = va_arg(ap, int);
        va_end(ap);
        return ::open(path, flags, mode);
    }
}


template< int Errno >
static pid_t
pipe_fail(int fildes[2])
{
    errno = Errno;
    return -1;
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(child_with_files__ok_function);
ATF_TEST_CASE_BODY(child_with_files__ok_function)
{
    std::auto_ptr< process::child_with_files > child =
        process::child_with_files::fork(
            child_simple_function< 15, 'Z' >,
            fs::path("file1.txt"), fs::path("file2.txt"));
    const process::status status = child->wait();
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(15, status.exitstatus());

    ATF_REQUIRE(utils::grep_file("^To stdout: Z$",
                                 fs::path("file1.txt")));
    ATF_REQUIRE(utils::grep_file("^To stderr: Z",
                                 fs::path("file2.txt")));
}


ATF_TEST_CASE_WITHOUT_HEAD(child_with_files__ok_functor);
ATF_TEST_CASE_BODY(child_with_files__ok_functor)
{
    std::auto_ptr< process::child_with_files > child =
        process::child_with_files::fork(
            child_simple_functor(16, "a functor"),
            fs::path("fileA.txt"), fs::path("fileB.txt"));
    const process::status status = child->wait();
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(16, status.exitstatus());

    ATF_REQUIRE(utils::grep_file("^To stdout: a functor",
                                 fs::path("fileA.txt")));
    ATF_REQUIRE(utils::grep_file("^To stderr: a functor$",
                                 fs::path("fileB.txt")));
}


ATF_TEST_CASE_WITHOUT_HEAD(child_with_files__wait_timeout_ok);
ATF_TEST_CASE_BODY(child_with_files__wait_timeout_ok)
{
    std::auto_ptr< process::child_with_files > child =
        process::child_with_files::fork(
            child_wait< 500000 >, fs::path("out"), fs::path("err"));
    const process::status status = child->wait(datetime::delta(5, 0));
    ATF_REQUIRE(fs::exists(fs::path("finished")));
}


ATF_TEST_CASE_WITHOUT_HEAD(child_with_files__wait_timeout_expired);
ATF_TEST_CASE_BODY(child_with_files__wait_timeout_expired)
{
    std::auto_ptr< process::child_with_files > child =
        process::child_with_files::fork(
            child_wait_with_subchild< 500000 >, fs::path("out"),
            fs::path("err"));
    ATF_REQUIRE_THROW(process::timeout_error,
                      child->wait(datetime::delta(0, 50000)));
    ATF_REQUIRE(!fs::exists(fs::path("finished")));

    // Check that the subprocess of the child is also killed.
    ::sleep(1);
    ATF_REQUIRE(!fs::exists(fs::path("finished")));
    ATF_REQUIRE(!fs::exists(fs::path("subfinished")));
}


ATF_TEST_CASE_WITHOUT_HEAD(child_with_files__fork_fail);
ATF_TEST_CASE_BODY(child_with_files__fork_fail)
{
    process::detail::syscall_fork = fork_fail< 1234 >;
    try {
        process::child_with_files::fork(child_simple_function< 1, 'A' >,
                                        fs::path("a.txt"),
                                        fs::path("b.txt"));
        fail("Expected exception but none raised");
    } catch (const process::system_error& e) {
        ATF_REQUIRE(utils::grep_string("fork.*failed", e.what()));
        ATF_REQUIRE_EQ(1234, e.original_errno());
    }
    ATF_REQUIRE(!fs::exists(fs::path("a.txt")));
    ATF_REQUIRE(!fs::exists(fs::path("b.txt")));
}


ATF_TEST_CASE_WITHOUT_HEAD(child_with_files__create_stdout_fail);
ATF_TEST_CASE_BODY(child_with_files__create_stdout_fail)
{
    process::detail::syscall_open = open_fail< ENOENT >;
    std::auto_ptr< process::child_with_files > child =
        process::child_with_files::fork(child_simple_function< 1, 'A' >,
                                        fs::path("raise-error"),
                                        fs::path("created"));
    const process::status status = child->wait();
    ATF_REQUIRE(status.signaled());
    ATF_REQUIRE_EQ(SIGABRT, status.termsig());
    ATF_REQUIRE(!fs::exists(fs::path("raise-error")));
    ATF_REQUIRE(!fs::exists(fs::path("created")));
}


ATF_TEST_CASE_WITHOUT_HEAD(child_with_files__create_stderr_fail);
ATF_TEST_CASE_BODY(child_with_files__create_stderr_fail)
{
    process::detail::syscall_open = open_fail< ENOENT >;
    std::auto_ptr< process::child_with_files > child =
        process::child_with_files::fork(child_simple_function< 1, 'A' >,
                                        fs::path("created"),
                                        fs::path("raise-error"));
    const process::status status = child->wait();
    ATF_REQUIRE(status.signaled());
    ATF_REQUIRE_EQ(SIGABRT, status.termsig());
    ATF_REQUIRE(fs::exists(fs::path("created")));
    ATF_REQUIRE(!fs::exists(fs::path("raise-error")));
}


template< class Hook >
static void
child_with_output__ok(Hook hook)
{
    std::cout << "This unflushed message should not propagate to the child";
    std::cerr << "This unflushed message should not propagate to the child";
    std::auto_ptr< process::child_with_output > child =
        process::child_with_output::fork(hook);
    std::cout << std::endl;
    std::cerr << std::endl;

    std::istream& output = child->output();
    for (std::size_t i = 0; i < 100; i++) {
        std::string line;
        ATF_REQUIRE(std::getline(output, line).good());
        ATF_REQUIRE_EQ((F("This is a message to stdout, "
                          "sequence %d") % i).str(), line);
    }

    std::string line;
    ATF_REQUIRE(std::getline(output, line).good());
    ATF_REQUIRE_EQ("Exiting", line);

    process::status status = child->wait();
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(EXIT_SUCCESS, status.exitstatus());
}


ATF_TEST_CASE_WITHOUT_HEAD(child_with_output__ok_function);
ATF_TEST_CASE_BODY(child_with_output__ok_function)
{
    child_with_output__ok(child_printer_function);
}


ATF_TEST_CASE_WITHOUT_HEAD(child_with_output__ok_functor);
ATF_TEST_CASE_BODY(child_with_output__ok_functor)
{
    child_with_output__ok(child_printer_functor());
}


ATF_TEST_CASE_WITHOUT_HEAD(child_with_output__wait_timeout_ok);
ATF_TEST_CASE_BODY(child_with_output__wait_timeout_ok)
{
    std::auto_ptr< process::child_with_output > child =
        process::child_with_output::fork(child_wait< 500000 >);
    const process::status status = child->wait(datetime::delta(5, 0));
    ATF_REQUIRE(fs::exists(fs::path("finished")));
}


ATF_TEST_CASE_WITHOUT_HEAD(child_with_output__wait_timeout_expired);
ATF_TEST_CASE_BODY(child_with_output__wait_timeout_expired)
{
    std::auto_ptr< process::child_with_output > child =
        process::child_with_output::fork(child_wait_with_subchild< 500000 >);
    ATF_REQUIRE_THROW(process::timeout_error,
                      child->wait(datetime::delta(0, 50000)));
    ATF_REQUIRE(!fs::exists(fs::path("finished")));

    // Check that the subprocess of the child is also killed.
    ::sleep(1);
    ATF_REQUIRE(!fs::exists(fs::path("finished")));
    ATF_REQUIRE(!fs::exists(fs::path("subfinished")));
}


ATF_TEST_CASE_WITHOUT_HEAD(child_with_output__pipe_fail);
ATF_TEST_CASE_BODY(child_with_output__pipe_fail)
{
    process::detail::syscall_pipe = pipe_fail< 23 >;
    try {
        process::child_with_output::fork(child_simple_function< 1, 'A' >);
        fail("Expected exception but none raised");
    } catch (const process::system_error& e) {
        ATF_REQUIRE(utils::grep_string("pipe.*failed", e.what()));
        ATF_REQUIRE_EQ(23, e.original_errno());
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(child_with_output__fork_fail);
ATF_TEST_CASE_BODY(child_with_output__fork_fail)
{
    process::detail::syscall_fork = fork_fail< 89 >;
    try {
        process::child_with_output::fork(child_simple_function< 1, 'A' >);
        fail("Expected exception but none raised");
    } catch (const process::system_error& e) {
        ATF_REQUIRE(utils::grep_string("fork.*failed", e.what()));
        ATF_REQUIRE_EQ(89, e.original_errno());
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(exec__absolute_path);
ATF_TEST_CASE_BODY(exec__absolute_path)
{
    std::vector< std::string > args;
    args.push_back("return-code");
    args.push_back("12");

    const fs::path program = get_helpers(this);
    INV(program.is_absolute());
    std::auto_ptr< process::child_with_files > child =
        process::child_with_files::fork(do_exec(program, args),
                                        fs::path("out"), fs::path("err"));

    const process::status status = child->wait();
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(12, status.exitstatus());
}


ATF_TEST_CASE_WITHOUT_HEAD(exec__relative_path);
ATF_TEST_CASE_BODY(exec__relative_path)
{
    std::vector< std::string > args;
    args.push_back("return-code");
    args.push_back("13");

    ATF_REQUIRE(::mkdir("root", 0755) != -1);
    ATF_REQUIRE(::symlink(get_helpers(this).c_str(), "root/helpers") != -1);

    std::auto_ptr< process::child_with_files > child =
        process::child_with_files::fork(do_exec(fs::path("root/helpers"), args),
                                        fs::path("out"), fs::path("err"));

    const process::status status = child->wait();
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(13, status.exitstatus());
}


ATF_TEST_CASE_WITHOUT_HEAD(exec__basename_only);
ATF_TEST_CASE_BODY(exec__basename_only)
{
    std::vector< std::string > args;
    args.push_back("return-code");
    args.push_back("14");

    ATF_REQUIRE(::symlink(get_helpers(this).c_str(), "helpers") != -1);

    std::auto_ptr< process::child_with_files > child =
        process::child_with_files::fork(do_exec(fs::path("helpers"), args),
                                        fs::path("out"), fs::path("err"));

    const process::status status = child->wait();
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(14, status.exitstatus());
}


ATF_TEST_CASE_WITHOUT_HEAD(exec__no_path);
ATF_TEST_CASE_BODY(exec__no_path)
{
    std::vector< std::string > args;
    args.push_back("return-code");
    args.push_back("14");

    const fs::path helpers = get_helpers(this);
    utils::setenv("PATH", helpers.branch_path().c_str());
    std::auto_ptr< process::child_with_output > child =
        process::child_with_output::fork(do_exec(fs::path(helpers.leaf_name()),
                                                 args));

    std::string line;
    ATF_REQUIRE(std::getline(child->output(), line).good());
    ATF_REQUIRE_MATCH("Failed to execute", line);
    ATF_REQUIRE(!std::getline(child->output(), line));

    const process::status status = child->wait();
    ATF_REQUIRE(status.signaled());
    ATF_REQUIRE_EQ(SIGABRT, status.termsig());
}


ATF_TEST_CASE_WITHOUT_HEAD(exec__no_args);
ATF_TEST_CASE_BODY(exec__no_args)
{
    std::vector< std::string > args;
    std::auto_ptr< process::child_with_output > child =
        process::child_with_output::fork(do_exec(get_helpers(this), args));

    std::string line;
    ATF_REQUIRE(std::getline(child->output(), line).good());
    ATF_REQUIRE_EQ("Must provide a helper name", line);
    ATF_REQUIRE(!std::getline(child->output(), line));

    const process::status status = child->wait();
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(EXIT_FAILURE, status.exitstatus());
}


ATF_TEST_CASE_WITHOUT_HEAD(exec__some_args);
ATF_TEST_CASE_BODY(exec__some_args)
{
    std::vector< std::string > args;
    args.push_back("print-args");
    args.push_back("foo");
    args.push_back("   bar baz ");
    std::auto_ptr< process::child_with_output > child =
        process::child_with_output::fork(do_exec(get_helpers(this), args));

    std::string line;
    ATF_REQUIRE(std::getline(child->output(), line).good());
    ATF_REQUIRE_EQ("argv[0] = " + get_helpers(this).str(), line);
    ATF_REQUIRE(std::getline(child->output(), line).good());
    ATF_REQUIRE_EQ("argv[1] = print-args", line);
    ATF_REQUIRE(std::getline(child->output(), line));
    ATF_REQUIRE_EQ("argv[2] = foo", line);
    ATF_REQUIRE(std::getline(child->output(), line));
    ATF_REQUIRE_EQ("argv[3] =    bar baz ", line);
    ATF_REQUIRE(std::getline(child->output(), line));
    ATF_REQUIRE_EQ("argv[4] = NULL", line);
    ATF_REQUIRE(!std::getline(child->output(), line));

    const process::status status = child->wait();
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(EXIT_SUCCESS, status.exitstatus());
}


ATF_TEST_CASE_WITHOUT_HEAD(exec__missing_program);
ATF_TEST_CASE_BODY(exec__missing_program)
{
    std::vector< std::string > args;
    std::auto_ptr< process::child_with_output > child =
        process::child_with_output::fork(do_exec(fs::path("a/b/c"), args));

    std::string line;
    ATF_REQUIRE(std::getline(child->output(), line).good());
    const std::string exp = "Failed to execute a/b/c: ";
    ATF_REQUIRE_EQ(exp, line.substr(0, exp.length()));
    ATF_REQUIRE(!std::getline(child->output(), line));

    const process::status status = child->wait();
    ATF_REQUIRE(status.signaled());
    ATF_REQUIRE_EQ(SIGABRT, status.termsig());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, child_with_files__ok_function);
    ATF_ADD_TEST_CASE(tcs, child_with_files__ok_functor);
    ATF_ADD_TEST_CASE(tcs, child_with_files__wait_timeout_ok);
    ATF_ADD_TEST_CASE(tcs, child_with_files__wait_timeout_expired);
    ATF_ADD_TEST_CASE(tcs, child_with_files__fork_fail);
    ATF_ADD_TEST_CASE(tcs, child_with_files__create_stdout_fail);
    ATF_ADD_TEST_CASE(tcs, child_with_files__create_stderr_fail);

    ATF_ADD_TEST_CASE(tcs, child_with_output__ok_function);
    ATF_ADD_TEST_CASE(tcs, child_with_output__ok_functor);
    ATF_ADD_TEST_CASE(tcs, child_with_output__wait_timeout_ok);
    ATF_ADD_TEST_CASE(tcs, child_with_output__wait_timeout_expired);
    ATF_ADD_TEST_CASE(tcs, child_with_output__pipe_fail);
    ATF_ADD_TEST_CASE(tcs, child_with_output__fork_fail);

    ATF_ADD_TEST_CASE(tcs, exec__absolute_path);
    ATF_ADD_TEST_CASE(tcs, exec__relative_path);
    ATF_ADD_TEST_CASE(tcs, exec__basename_only);
    ATF_ADD_TEST_CASE(tcs, exec__no_path);
    ATF_ADD_TEST_CASE(tcs, exec__no_args);
    ATF_ADD_TEST_CASE(tcs, exec__some_args);
    ATF_ADD_TEST_CASE(tcs, exec__missing_program);
}
