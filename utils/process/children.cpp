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

#include "utils/process/children.ipp"

extern "C" {
#include <sys/stat.h>
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
        throw process::system_error(F("dup2(%s, %s) failed") % old_fd % new_fd,
                                    original_errno);
    }
}


/// Exception-based version of open(2) to open (or create) a file for append.
///
/// \param filename The file to open in append mode.
///
/// \return The file descriptor for the opened or created file.
///
/// \throw process::system_error If the call to open(2) fails.
static int
open_for_append(const fs::path& filename)
{
    const int fd = process::detail::syscall_open(
        filename.c_str(), O_CREAT | O_WRONLY | O_APPEND,
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
/// Because we are waiting for the termination of a process, and because this is
/// the canonical way to call wait(2) for this module, we ensure from here that
/// any subprocess of the process we are killing is terminated.
///
/// \param pid The identifier of the process to wait for.
///
/// \return The termination status of the process.
///
/// \throw process::system_error If the call to waitpid(2) fails.
static process::status
safe_wait(const pid_t pid)
{
    LD(F("Waiting for pid=%s, no timeout") % pid);
    int stat_loc;
    if (process::detail::syscall_waitpid(pid, &stat_loc, 0) == -1) {
        const int original_errno = errno;
        throw process::system_error(F("Failed to wait for PID %s") % pid,
                                    original_errno);
    }
    LD(F("Sending KILL signal to process group %s") % pid);
retry:
    if (::killpg(pid, SIGKILL) == -1) {
        if (errno == EINTR)
            goto retry;
        // Otherwise, just ignore the error and continue.  It should not have
        // happened.
    }
    return process::status(pid, stat_loc);
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
    LD(F("Waiting for pid=%s: timeout seconds=%s, useconds=%s") % pid %
       timeout.seconds % timeout.useconds);

    timed_wait__aux::fired = false;
    timed_wait__aux::pid = pid;
    signals::timer timer(timeout, timed_wait__aux::callback);
    try {
        const process::status status = safe_wait(pid);
        timer.unprogram();
        return status;
    } catch (const process::system_error& error) {
        if (error.original_errno() == EINTR) {
            if (timed_wait__aux::fired) {
                timer.unprogram();
                (void)safe_wait(pid);
                throw process::timeout_error(
                    F("The timeout was exceeded while waiting for process "
                      "%s; forcibly killed") % pid);
            } else
                throw error;
        } else
            throw error;
    }
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
///     If this has the magic value /dev/stdout, then the parent's stdout is
///     reused without applying any redirection.
/// \param stderr_file The name of the file in which to store the stderr.
///     If this has the magic value /dev/stderr, then the parent's stderr is
///     reused without applying any redirection.
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
        ::setpgid(::getpid(), ::getpid());

        try {
            if (stdout_file != fs::path("/dev/stdout")) {
                const int stdout_fd = open_for_append(stdout_file);
                safe_dup(stdout_fd, STDOUT_FILENO);
                ::close(stdout_fd);
            }
            if (stderr_file != fs::path("/dev/stderr")) {
                const int stderr_fd = open_for_append(stderr_file);
                safe_dup(stderr_fd, STDERR_FILENO);
                ::close(stderr_fd);
            }
        } catch (const system_error& e) {
            std::cerr << F("Failed to set up subprocess: %s\n") % e.what();
            std::abort();
        }
        return std::auto_ptr< process::child_with_files >(NULL);
    } else {
        LD(F("Spawned process %s: stdout=%s, stderr=%s") % pid % stdout_file %
           stderr_file);
        return std::auto_ptr< process::child_with_files >(
            new process::child_with_files(new impl(pid)));
    }
}


/// Returns the process identifier of this child.
///
/// \return A process identifier.
int
process::child_with_files::pid(void) const
{
    return _pimpl->_pid;
}


/// Blocks to wait for completion.
///
/// Note that this does not loop in case the wait call is interrupted.  We need
/// callers to know when this condition happens and let them retry on their own.
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
        ::setpgid(::getpid(), ::getpid());

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
        LD(F("Spawned process %s: stdout and stderr inherited") % pid);
        return std::auto_ptr< process::child_with_output >(
            new process::child_with_output(new impl(
                pid, new process::ifdstream(fds[0]))));
    }
}


/// Returns the process identifier of this child.
///
/// \return A process identifier.
int
process::child_with_output::pid(void) const
{
    return _pimpl->_pid;
}


/// Blocks to wait for completion.
///
/// Note that this does not loop in case the wait call is interrupted.  We need
/// callers to know when this condition happens and let them retry on their own.
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
/// \param program The binary to execute.
/// \param args The arguments to pass to the binary, without the program name.
///
/// \throw process::system_error If the call to exec(3) fails.
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

    for (char** arg = argv; *arg != NULL; arg++)
        delete *arg;
    delete [] argv;

    throw system_error(F("Failed to execute %s") % program, original_errno);
}
