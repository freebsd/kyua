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
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
}

#include <cstdarg>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

#include <atf-c++.hpp>

#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/logging/macros.hpp"
#include "utils/process/children.ipp"
#include "utils/process/exceptions.hpp"
#include "utils/process/system.hpp"
#include "utils/sanity.hpp"
#include "utils/signals/timer.hpp"
#include "utils/test_utils.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace logging = utils::logging;
namespace process = utils::process;
namespace signals = utils::signals;


namespace {


/// Process that the timer will terminate.
static int timer_pid = 0;


/// Callback for a timer to set timer_fired to true.
static void
timer_callback(void)
{
    ::kill(timer_pid, SIGCONT);
}


/// Validates that interrupting the wait call raises the proper error.
///
/// \param child The child to validate.
template< class Child >
void
interrupted_check(Child& child)
{
    timer_pid = ::getpid();
    signals::timer timer(datetime::delta(0, 500000), timer_callback);

    std::cout << "Waiting for subprocess; should be aborted\n";
    ATF_REQUIRE_THROW(process::system_error,
                      child->wait(datetime::delta()));

    timer.unprogram();

    std::cout << "Now terminating process for real\n";
    ::kill(child->pid(), SIGKILL);
    const process::status status = child->wait(datetime::delta());
    ATF_REQUIRE(status.signaled());

    ATF_REQUIRE(!fs::exists(fs::path("finished")));
}


/// Body for a process that spawns a subprocess.
///
/// This is supposed to be passed as a hook to one of the fork() functions.  The
/// fork() functions run their children in a new process group, so it is
/// expected that the subprocess we spawn here is part of this process group as
/// well.
static void
child_blocking_subchild(void)
{
    pid_t pid = ::fork();
    if (pid == -1) {
        std::abort();
    } else if (pid == 0) {
        for (;;)
            ::pause();
    } else {
        std::ofstream output("subchild_pid");
        if (!output)
            std::abort();
        output << pid << "\n";
        output.close();
        std::exit(EXIT_SUCCESS);
    }
    UNREACHABLE;
}


/// Ensures that the subprocess started by child_blocking_subchild is dead.
///
/// This function has to be called after running the child_blocking_subchild
/// function through a fork call.  It ensures that the subchild spawned is
/// ready, waits for the process group and ensures that both the child and the
/// subchild have died.
///
/// \param child The child object.
template< class Child >
void
child_blocking_subchild_check(Child child)
{
    const process::status status = child->wait();

    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(EXIT_SUCCESS, status.exitstatus());

    std::ifstream input("subchild_pid");
    ATF_REQUIRE(input);
    pid_t pid;
    input >> pid;
    input.close();
    std::cout << F("Subprocess was %d; checking if it died\n") % pid;

    int retries = 3;
retry:
    if (::kill(pid, SIGCONT) != -1 || errno != ESRCH) {
        // Looks like the subchild did not die.  Note that this might be
        // inaccurate: the system may have spawned a new process with the same
        // pid as our subchild... but in practice, this does not happen because
        // most systems do not immediately reuse pid numbers.
        if (retries > 0) {
            std::cout << "Subprocess not dead yet; retrying wait\n";
            ::sleep(1);
            retries--;
            goto retry;
        }
        ATF_FAIL(F("The subprocess %d of our child was not killed") % pid);
    }
}


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
    std::cout << "Sleeping in subprocess\n";
    if (Microseconds > 1000000)
        ::sleep(Microseconds / 1000000);
    else
        ::usleep(Microseconds);
    std::cout << "Resuming subprocess and exiting\n";
    utils::create_file(fs::path("finished"));
    std::exit(EXIT_SUCCESS);
}


template< int Microseconds >
static void
child_wait_with_subchild(void)
{
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


/// Body for a child process that creates a pidfile.
static void
child_write_pid(void)
{
    std::ofstream output("pidfile");
    output << ::getpid() << "\n";
    output.close();
    std::exit(EXIT_SUCCESS);
}


/// Validates that the value of the pidfile matches the pid file in the child.
///
/// \param child The child to validate.
template< class Child >
void
child_write_pid_check(Child& child)
{
    const int pid = child->pid();

    const process::status status = child->wait();
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(EXIT_SUCCESS, status.exitstatus());

    std::ifstream input("pidfile");
    ATF_REQUIRE(input);
    int read_pid;
    input >> read_pid;
    input.close();

    ATF_REQUIRE_EQ(read_pid, pid);
}


/// A child process that returns.
///
/// The fork() wrappers are supposed to capture this condition and terminate the
/// child before the code returns to the fork() call point.
static void
child_return(void)
{
}


/// A child process that raises an exception.
///
/// The fork() wrappers are supposed to capture this condition and terminate the
/// child before the code returns to the fork() call point.
template< class Type, Type Value >
void
child_raise_exception(void)
{
    throw Type(Value);
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
        logging::set_inmemory();
        try {
            process::exec(_program, _args);
        } catch (const process::system_error& e) {
            std::cerr << "Caught system_error: " << e.what() << '\n';
            std::abort();
        }
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
pipe_fail(int* UTILS_UNUSED_PARAM(fildes))
{
    errno = Errno;
    return -1;
}


/// Helper for child_with_files tests to validate inheritance of stdout/stderr.
///
/// This function ensures that passing one of /dev/stdout or /dev/stderr to
/// the child_with_files fork method does the right thing.  The idea is that we
/// call fork with the given parameters and then make our child redirect one of
/// its file descriptors to a specific file without going through the process
/// library.  We then validate if this redirection worked and got the expected
/// output.
///
/// \param fork_stdout The path to pass to the fork call as the stdout file.
/// \param fork_stderr The path to pass to the fork call as the stderr file.
/// \param child_file The file to explicitly in the subchild.
/// \param child_fd The file descriptor to which to attach child_file.
static void
do_inherit_test(const char* fork_stdout, const char* fork_stderr,
                const char* child_file, const int child_fd)
{
    const pid_t pid = ::fork();
    ATF_REQUIRE(pid != -1);
    if (pid == 0) {
        const int fd = ::open(child_file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd != child_fd) {
            if (::dup2(fd, child_fd) == -1)
                std::abort();
            ::close(fd);
        }

        std::auto_ptr< process::child_with_files > child =
            process::child_with_files::fork(
                child_simple_function< 123, 'Z' >,
                fs::path(fork_stdout), fs::path(fork_stderr));
        const process::status status = child->wait();
        if (!status.exited() || status.exitstatus() != 123)
            std::abort();
        std::exit(EXIT_SUCCESS);
    } else {
        int status;
        ATF_REQUIRE(::waitpid(pid, &status, 0) != -1);
        ATF_REQUIRE(WIFEXITED(status));
        ATF_REQUIRE_EQ(EXIT_SUCCESS, WEXITSTATUS(status));
        ATF_REQUIRE(utils::grep_file("stdout: Z", fs::path("stdout.txt")));
        ATF_REQUIRE(utils::grep_file("stderr: Z", fs::path("stderr.txt")));
    }
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


ATF_TEST_CASE_WITHOUT_HEAD(child_with_files__pid);
ATF_TEST_CASE_BODY(child_with_files__pid)
{
    std::auto_ptr< process::child_with_files > child =
        process::child_with_files::fork(
            child_write_pid, fs::path("file1.txt"), fs::path("file2.txt"));

    child_write_pid_check(child);
}


ATF_TEST_CASE_WITHOUT_HEAD(child_with_files__inherit_stdout);
ATF_TEST_CASE_BODY(child_with_files__inherit_stdout)
{
    do_inherit_test("/dev/stdout", "stderr.txt", "stdout.txt", STDOUT_FILENO);
}


ATF_TEST_CASE_WITHOUT_HEAD(child_with_files__inherit_stderr);
ATF_TEST_CASE_BODY(child_with_files__inherit_stderr)
{
    do_inherit_test("stdout.txt", "/dev/stderr", "stderr.txt", STDERR_FILENO);
}


ATF_TEST_CASE_WITHOUT_HEAD(child_with_files__wait_killpg);
ATF_TEST_CASE_BODY(child_with_files__wait_killpg)
{
    std::auto_ptr< process::child_with_files > child =
        process::child_with_files::fork(child_blocking_subchild,
                                        fs::path("out"), fs::path("err"));

    child_blocking_subchild_check(child);
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


ATF_TEST_CASE_WITHOUT_HEAD(child_with_files__interrupted);
ATF_TEST_CASE_BODY(child_with_files__interrupted)
{
    std::auto_ptr< process::child_with_files > child =
        process::child_with_files::fork(child_wait< 30000000 >,
                                        fs::path("out"), fs::path("err"));

    interrupted_check(child);
}


ATF_TEST_CASE_WITHOUT_HEAD(child_with_files__fork_cannot_exit);
ATF_TEST_CASE_BODY(child_with_files__fork_cannot_exit)
{
    const pid_t parent_pid = ::getpid();
    utils::create_file(fs::path("to-not-be-deleted"));

    std::auto_ptr< process::child_with_files > child =
        process::child_with_files::fork(child_return,
                                        fs::path("out"), fs::path("err"));
    if (::getpid() != parent_pid) {
        // If we enter this clause, it is because the hook returned.
        ::unlink("to-not-be-deleted");
        std::exit(EXIT_SUCCESS);
    }

    const process::status status = child->wait();
    ATF_REQUIRE(status.signaled());
    ATF_REQUIRE(fs::exists(fs::path("to-not-be-deleted")));
}


ATF_TEST_CASE_WITHOUT_HEAD(child_with_files__fork_cannot_unwind);
ATF_TEST_CASE_BODY(child_with_files__fork_cannot_unwind)
{
    const pid_t parent_pid = ::getpid();
    utils::create_file(fs::path("to-not-be-deleted"));
    try {
        std::auto_ptr< process::child_with_files > child =
            process::child_with_files::fork(child_raise_exception< int, 123 >,
                                            fs::path("out"), fs::path("err"));
        const process::status status = child->wait();
        ATF_REQUIRE(status.signaled());
        ATF_REQUIRE(fs::exists(fs::path("to-not-be-deleted")));
    } catch (const int i) {
        // If we enter this clause, it is because an exception leaked from the
        // hook.
        INV(parent_pid != ::getpid());
        INV(i == 123);
        ::unlink("to-not-be-deleted");
        std::exit(EXIT_SUCCESS);
    }
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


ATF_TEST_CASE_WITHOUT_HEAD(child_with_output__pid);
ATF_TEST_CASE_BODY(child_with_output__pid)
{
    std::auto_ptr< process::child_with_output > child =
        process::child_with_output::fork(child_write_pid);

    child_write_pid_check(child);
}


ATF_TEST_CASE_WITHOUT_HEAD(child_with_output__wait_killpg);
ATF_TEST_CASE_BODY(child_with_output__wait_killpg)
{
    std::auto_ptr< process::child_with_output > child =
        process::child_with_output::fork(child_blocking_subchild);
    child_blocking_subchild_check(child);
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


ATF_TEST_CASE_WITHOUT_HEAD(child_with_output__interrupted);
ATF_TEST_CASE_BODY(child_with_output__interrupted)
{
    std::auto_ptr< process::child_with_output > child =
        process::child_with_output::fork(child_wait< 30000000 >);

    interrupted_check(child);
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


ATF_TEST_CASE_WITHOUT_HEAD(child_with_output__fork_cannot_exit);
ATF_TEST_CASE_BODY(child_with_output__fork_cannot_exit)
{
    const pid_t parent_pid = ::getpid();
    utils::create_file(fs::path("to-not-be-deleted"));

    std::auto_ptr< process::child_with_output > child =
        process::child_with_output::fork(child_return);
    if (::getpid() != parent_pid) {
        // If we enter this clause, it is because the hook returned.
        ::unlink("to-not-be-deleted");
        std::exit(EXIT_SUCCESS);
    }

    const process::status status = child->wait();
    ATF_REQUIRE(status.signaled());
    ATF_REQUIRE(fs::exists(fs::path("to-not-be-deleted")));
}


ATF_TEST_CASE_WITHOUT_HEAD(child_with_output__fork_cannot_unwind);
ATF_TEST_CASE_BODY(child_with_output__fork_cannot_unwind)
{
    const pid_t parent_pid = ::getpid();
    utils::create_file(fs::path("to-not-be-deleted"));
    try {
        std::auto_ptr< process::child_with_output > child =
            process::child_with_output::fork(child_raise_exception< int, 123 >);
        const process::status status = child->wait();
        ATF_REQUIRE(status.signaled());
        ATF_REQUIRE(fs::exists(fs::path("to-not-be-deleted")));
    } catch (const int i) {
        // If we enter this clause, it is because an exception leaked from the
        // hook.
        INV(parent_pid != ::getpid());
        INV(i == 123);
        ::unlink("to-not-be-deleted");
        std::exit(EXIT_SUCCESS);
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
    logging::set_inmemory();

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
    const std::string exp = "Caught system_error: Failed to execute a/b/c: ";
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
    ATF_ADD_TEST_CASE(tcs, child_with_files__pid);
    ATF_ADD_TEST_CASE(tcs, child_with_files__inherit_stdout);
    ATF_ADD_TEST_CASE(tcs, child_with_files__inherit_stderr);
    ATF_ADD_TEST_CASE(tcs, child_with_files__wait_killpg);
    ATF_ADD_TEST_CASE(tcs, child_with_files__wait_timeout_ok);
    ATF_ADD_TEST_CASE(tcs, child_with_files__wait_timeout_expired);
    ATF_ADD_TEST_CASE(tcs, child_with_files__interrupted);
    ATF_ADD_TEST_CASE(tcs, child_with_files__fork_cannot_exit);
    ATF_ADD_TEST_CASE(tcs, child_with_files__fork_cannot_unwind);
    ATF_ADD_TEST_CASE(tcs, child_with_files__fork_fail);
    ATF_ADD_TEST_CASE(tcs, child_with_files__create_stdout_fail);
    ATF_ADD_TEST_CASE(tcs, child_with_files__create_stderr_fail);

    ATF_ADD_TEST_CASE(tcs, child_with_output__ok_function);
    ATF_ADD_TEST_CASE(tcs, child_with_output__ok_functor);
    ATF_ADD_TEST_CASE(tcs, child_with_output__wait_killpg);
    ATF_ADD_TEST_CASE(tcs, child_with_output__wait_timeout_ok);
    ATF_ADD_TEST_CASE(tcs, child_with_output__wait_timeout_expired);
    ATF_ADD_TEST_CASE(tcs, child_with_output__interrupted);
    ATF_ADD_TEST_CASE(tcs, child_with_output__pipe_fail);
    ATF_ADD_TEST_CASE(tcs, child_with_output__fork_cannot_exit);
    ATF_ADD_TEST_CASE(tcs, child_with_output__fork_cannot_unwind);
    ATF_ADD_TEST_CASE(tcs, child_with_output__fork_fail);

    ATF_ADD_TEST_CASE(tcs, exec__absolute_path);
    ATF_ADD_TEST_CASE(tcs, exec__relative_path);
    ATF_ADD_TEST_CASE(tcs, exec__basename_only);
    ATF_ADD_TEST_CASE(tcs, exec__no_path);
    ATF_ADD_TEST_CASE(tcs, exec__no_args);
    ATF_ADD_TEST_CASE(tcs, exec__some_args);
    ATF_ADD_TEST_CASE(tcs, exec__missing_program);
}
