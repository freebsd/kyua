// Copyright 2010 The Kyua Authors.
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

#include "utils/process/child.ipp"

extern "C" {
#include <sys/stat.h>
#include <sys/wait.h>

#include <signal.h>
#include <unistd.h>
}

#include <cerrno>
#include <iostream>
#include <memory>

#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/noncopyable.hpp"
#include "utils/process/exceptions.hpp"
#include "utils/process/fdstream.hpp"
#include "utils/process/operations.hpp"
#include "utils/process/system.hpp"
#include "utils/process/status.hpp"
#include "utils/sanity.hpp"
#include "utils/signals/interrupts.hpp"


namespace utils {
namespace process {


/// Private implementation fields for child objects.
struct child::impl : utils::noncopyable {
    /// The process identifier.
    pid_t _pid;

    /// The input stream for the process' stdout and stderr.  May be NULL.
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


namespace fs = utils::fs;
namespace process = utils::process;
namespace signals = utils::signals;


/// Creates a new child.
///
/// \param implptr A dynamically-allocated impl object with the contents of the
///     new child.
process::child::child(impl *implptr) :
    _pimpl(implptr)
{
}


/// Destructor for child.
process::child::~child(void)
{
}


/// Spawns a new subprocess and multiplexes and captures its stdout and stderr.
///
/// If the subprocess cannot be completely set up for any reason, it attempts to
/// dump an error message to its stderr channel and it then calls std::abort().
///
/// \param hook The function to execute in the subprocess.  Must not return.
/// \param cookie Opaque data to pass to the hook.
///
/// \return A new child object, returned as a dynamically-allocated object
/// because children classes are unique and thus noncopyable.
///
/// \throw process::system_error If the process cannot be spawned due to a
///     system call error.
std::auto_ptr< child >
child::fork_capture(const void (*hook)(const void*), const void* cookie)
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
        utils_process_child_fork_capture(hook, cookie);
    } else {
        ::close(fds[1]);
        LD(F("Spawned process %s: stdout and stderr inherited") % pid);
        signals::add_pid_to_kill(pid);
        return std::auto_ptr< process::child >(
            new process::child(new impl(pid, new process::ifdstream(fds[0]))));
    }

    return child;
}


/// Spawns a new subprocess and redirects its stdout and stderr to files.
///
/// If the subprocess cannot be completely set up for any reason, it attempts to
/// dump an error message to its stderr channel and it then calls std::abort().
///
/// \param hook The function to execute in the subprocess.  Must not return.
/// \param cookie Opaque data to pass to the hook.
/// \param stdout_file The name of the file in which to store the stdout.
/// \param stderr_file The name of the file in which to store the stderr.
///
/// \return A new child object, returned as a dynamically-allocated object
/// because children classes are unique and thus noncopyable.
///
/// \throw process::system_error If the process cannot be spawned due to a
///     system call error.
std::auto_ptr< child >
child::fork_files(const void (*hook)(const void*), const void* cookie,
                  const fs::path& stdout_file, const fs::path& stderr_file)
{
    std::cout.flush();
    std::cerr.flush();

    const char* stdout_file_cstr = stdout_file.c_str();
    const char* stderr_file_cstr = stderr_file.c_str();

    pid_t pid = detail::syscall_fork();
    if (pid == -1) {
        throw process::system_error("fork(2) failed", errno);
    } else if (pid == 0) {
        utils_process_child_fork_files(hook, cookie,
                                       stdout_file_cstr, stderr_file_cstr);
    } else {
        LD(F("Spawned process %s: stdout=%s, stderr=%s") % pid % stdout_file %
           stderr_file);
        signals::add_pid_to_kill(pid);
        return std::auto_ptr< process::child >(
            new process::child(new impl(pid, NULL)));
    }

    return child;
}


/// Returns the process identifier of this child.
///
/// \return A process identifier.
int
process::child::pid(void) const
{
    return _pimpl->_pid;
}


/// Gets the input stream corresponding to the stdout and stderr of the child.
///
/// \pre The child must have been started by fork_capture().
///
/// \return A reference to the input stream connected to the output of the test
/// case.
std::istream&
process::child::output(void)
{
    PRE(_pimpl->_output.get() != NULL);
    return *_pimpl->_output;
}


/// Blocks to wait for completion.
///
/// \return The termination status of the child process.
///
/// \throw process::system_error If the call to waitpid(2) fails.
process::status
process::child::wait(void)
{
    return process::wait(_pimpl->_pid);
}
