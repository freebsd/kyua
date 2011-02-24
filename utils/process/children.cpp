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
#include <sys/wait.h>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
}

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "utils/datetime.hpp"
#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"
#include "utils/process/children.ipp"
#include "utils/process/exceptions.hpp"
#include "utils/process/fdstream.hpp"
#include "utils/process/system.hpp"
#include "utils/process/status.hpp"
#include "utils/sanity.hpp"
#include "utils/signals/timer.hpp"


namespace utils {
namespace process {


/// Private implementation fields for child_with_files.
struct child_with_files::impl {
    /// The process identifier.
    pid_t _pid;

    /// Initializes private implementation data.
    ///
    /// \param pid The process identifier.
    impl(const pid_t pid) : _pid(pid) {}
};


/// Private implementation fields for child_with_files.
struct child_with_output::impl {
    /// The process identifier.
    pid_t _pid;

    /// The input stream for the process' stdout and stderr.
    std::auto_ptr< process::ifdstream > _output;

    /// Initializes private implementation data.
    ///
    /// \param pid The process identifier.
    /// \param output The input stream.  Grabs ownership of the pointer.
    impl(const pid_t pid, process::ifdstream* output) :
        _pid(pid), _output(output) {}
};


}  // namespace process
}  // namespace utils


namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace process = utils::process;
namespace signals = utils::signals;


namespace {


/// Exception-based version of dup(2).
///
/// \param old_fd The file descriptor to duplicate.
/// \param new_fd The file descriptor to use as the duplicate.  This is
///     closed if it was open before the copy happens.
///
/// \throw process::system_error If the call to dup2(2) fails.
static void
safe_dup(const int old_fd, const int new_fd)
{
    if (process::detail::syscall_dup2(old_fd, new_fd) == -1) {
        const int original_errno = errno;
        throw process::system_error(F("dup2(%d, %d) failed") % old_fd % new_fd,
                                    original_errno);
    }
}


/// Exception-based version of open(2).
///
/// \param filename The file to create.
///
/// \return The file descriptor for the created file.
///
/// \throw process::system_error If the call to open(2) fails.
static int
create_file(const fs::path& filename)
{
    const int fd = process::detail::syscall_open(
        filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1) {
        const int original_errno = errno;
        throw process::system_error(F("Failed to create %s because open(2) "
                                      "failed") % filename, original_errno);
    }
    return fd;
}


/// Exception-based, type-improved version of wait(2).
///
/// \param pid The identifier of the process to wait for.
///
/// \return The termination status of the process.
///
/// \throw process::system_error If the call to waitpid(2) fails.
static process::status
safe_wait(const pid_t pid)
{
retry:
    LD(F("Waiting for pid=%d, no timeout") % pid);
    int stat_loc;
    if (process::detail::syscall_waitpid(pid, &stat_loc, 0) == -1) {
        const int original_errno = errno;
        if (original_errno == EINTR)
            goto retry;
        throw process::system_error(F("Failed to wait for PID %d") % pid,
                                    original_errno);
    }
    return process::status(stat_loc);
}


namespace timed_wait__aux {


/// Whether the timer fired or not.
static bool fired;


/// The process to be killed when the timer expires.
static pid_t pid;


/// The handler for the timer.
static void
callback(void)
{
    fired = true;
    ::kill(pid, SIGKILL);
}


}  // namespace child_timer


/// Waits for a process enforcing a deadline.
///
/// \param pid The identifier of the process to wait for.
/// \param timeout The timeout for the wait.  If the timeout is exceeded, the
///     child process and its process group are forcibly killed.
///
/// \return The exit status of the process.
///
/// throw process::timeout_error If the deadline is exceeded.
static process::status
timed_wait(const pid_t pid, const datetime::delta& timeout)
{
    LD(F("Waiting for pid=%d: timeout seconds=%d, useconds=%d") % pid %
       timeout.seconds % timeout.useconds);

    timed_wait__aux::fired = false;
    timed_wait__aux::pid = pid;
    signals::timer timer(timeout, timed_wait__aux::callback);
    const process::status status = safe_wait(pid);
    timer.unprogram();
    if (timed_wait__aux::fired) {
        LD(F("Process %d timed out; sending KILL signal") % pid);
        (void)::killpg(pid, SIGKILL);
        throw process::timeout_error(F("The timeout was exceeded while waiting "
            "for process %d; forcibly killed") % pid);
    }
    return status;
}


/// Replacement for strdup(3).
///
/// strdup(3) is not a standard function and, therefore, cannot be assumed to be
/// present in the std namespace.  Just reimplement it and use standard C++
/// memory allocation functions.
///
/// \param str The C string to duplicate.
///
/// \return The duplicated string.  Must be deleted with operator delete[].
char*
duplicate_cstring(const char* str)
{
    char* copy = new char[std::strlen(str) + 1];
    std::strcpy(copy, str);
    return copy;
}


}  // anonymous namespace


/// Creates a new child_with_files.
///
/// \param implptr A dynamically-allocated impl object with the contents of the
///     new child_with_files.
process::child_with_files::child_with_files(impl *implptr) :
    _pimpl(implptr)
{
}


/// Destructor for child_with_files.
process::child_with_files::~child_with_files(void)
{
}


/// Helper function for fork().
///
/// Please note: if you update this function to change the return type or to
/// raise different errors, do not forget to update fork() accordingly.
///
/// \param stdout_file The name of the file in which to store the stdout.
/// \param stderr_file The name of the file in which to store the stderr.
///
/// \return In the case of the parent, a new child_with_files object returned
/// as a dynamically-allocated object because children classes are unique and
/// thus noncopyable.  In the case of the child, a NULL pointer.
///
/// \throw process::system_error If the call to fork(2) fails.
std::auto_ptr< process::child_with_files >
process::child_with_files::fork_aux(const fs::path& stdout_file,
                                    const fs::path& stderr_file)
{
    std::cout.flush();
    std::cerr.flush();

    pid_t pid = detail::syscall_fork();
    if (pid == -1) {
        throw process::system_error("fork(2) failed", errno);
    } else if (pid == 0) {
        try {
            const int stdout_fd = create_file(stdout_file);
            const int stderr_fd = create_file(stderr_file);
            safe_dup(stdout_fd, STDOUT_FILENO);
            ::close(stdout_fd);
            safe_dup(stderr_fd, STDERR_FILENO);
            ::close(stderr_fd);
        } catch (const system_error& e) {
            std::cerr << F("Failed to set up subprocess: %s\n") % e.what();
            std::abort();
        }
        return std::auto_ptr< process::child_with_files >(NULL);
    } else {
        LD(F("Spawned process %d: stdout=%s, stderr=%s") % pid % stdout_file %
           stderr_file);
        return std::auto_ptr< process::child_with_files >(
            new process::child_with_files(new impl(pid)));
    }
}


/// Blocks to wait for completion.
///
/// \param timeout The timeout for the wait.  If zero, no timeout logic is
///     applied.
///
/// \return The termination status of the child process.
///
/// \throw process::system_error If the call to waitpid(2) fails.
/// \throw process::timeout_error If the timeout expires.
process::status
process::child_with_files::wait(const datetime::delta& timeout)
{
    if (timeout == datetime::delta())
        return safe_wait(_pimpl->_pid);
    else
        return timed_wait(_pimpl->_pid, timeout);
}


/// Creates a new child_with_output.
///
/// \param implptr A dynamically-allocated impl object with the contents of the
///     new child_with_output.
process::child_with_output::child_with_output(impl *implptr) :
    _pimpl(implptr)
{
}


/// Destructor for child_with_output.
process::child_with_output::~child_with_output(void)
{
}


/// Gets the input stream corresponding to the stdout and stderr of the child.
std::istream&
process::child_with_output::output(void)
{
    return *_pimpl->_output;
}


/// Helper function for fork().
///
/// Please note: if you update this function to change the return type or to
/// raise different errors, do not forget to update fork() accordingly.
///
/// \return In the case of the parent, a new child_with_output object returned
/// as a dynamically-allocated object because children classes are unique and
/// thus noncopyable.  In the case of the child, a NULL pointer.
///
/// \throw process::system_error If the calls to pipe(2) or fork(2) fail.
std::auto_ptr< process::child_with_output >
process::child_with_output::fork_aux(void)
{
    std::cout.flush();
    std::cerr.flush();

    int fds[2];
    if (detail::syscall_pipe(fds) == -1)
        throw process::system_error("pipe(2) failed", errno);

    pid_t pid = detail::syscall_fork();
    if (pid == -1) {
        ::close(fds[0]);
        ::close(fds[1]);
        throw process::system_error("fork(2) failed", errno);
    } else if (pid == 0) {
        try {
            ::close(fds[0]);
            safe_dup(fds[1], STDOUT_FILENO);
            safe_dup(fds[1], STDERR_FILENO);
            ::close(fds[1]);
        } catch (const system_error& e) {
            std::cerr << F("Failed to set up subprocess: %s\n") % e.what();
            std::abort();
        }
        return std::auto_ptr< process::child_with_output >(NULL);
    } else {
        ::close(fds[1]);
        LD(F("Spawned process %d: stdout and stderr inherited") % pid);
        return std::auto_ptr< process::child_with_output >(
            new process::child_with_output(new impl(
                pid, new process::ifdstream(fds[0]))));
    }
}


/// Blocks to wait for completion.
///
/// \param timeout The timeout for the wait.  If zero, no timeout logic is
///     applied.
///
/// \return The termination status of the child process.
///
/// \throw process::system_error If the call to waitpid(2) fails.
/// \throw process::timeout_error If the timeout expires.
process::status
process::child_with_output::wait(const datetime::delta& timeout)
{
    if (timeout == datetime::delta())
        return safe_wait(_pimpl->_pid);
    else
        return timed_wait(_pimpl->_pid, timeout);
}


/// Executes an external binary and replaces the current process.
///
/// If the new binary cannot be executed, this fails unconditionally: it dumps
/// an error to stderr and then aborts execution.
///
/// \param program The binary to execute.
/// \param args The arguments to pass to the binary, without the program name.
void
process::exec(const fs::path& program, const std::vector< std::string >& args)
{
    char** argv = new char*[1 + args.size() + 1];

    argv[0] = duplicate_cstring(program.c_str());
    for (std::vector< std::string >::size_type i = 0; i < args.size(); i++)
        argv[1 + i] = duplicate_cstring(args[i].c_str());
    argv[1 + args.size()] = NULL;

    std::string plain_command;
    for (char** arg = argv; *arg != NULL; arg++)
        plain_command += F(" %s") % *arg;
    LD(F("Executing%s") % plain_command);

    const int ret = ::execv(program.c_str(), argv);
    const int original_errno = errno;
    INV(ret == -1);
    std::cerr << F("Failed to execute %s: %s\n") % program %
        std::strerror(original_errno);
    std::abort();
}
