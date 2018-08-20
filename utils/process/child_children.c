// Copyright 2017 The Kyua Authors.
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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils/defs.hpp"
#include "utils/process/child_children.h"


/// Syntactic sugar to call write(2) without specifying the length.
///
/// \param fd Output file descriptor.
/// \param message Message to write.
static inline void
do_write(const int fd, const char* message)
{
    const ssize_t ret = write(fd, message, strlen(message));
    assert(ret == strlen(message));
}


/// Runs open(2) to open (or create) a file for append and exits on error.
///
/// \param filename The file to open in append mode.
///
/// \return The file descriptor for the opened or created file.
static int
open_for_append(const char* filename)
{
    const int fd = open(filename, O_CREAT | O_WRONLY | O_APPEND,
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1) {
        const int original_errno = errno;
        do_write(STDERR_FILENO, "Failed to create ");
        do_write(STDERR_FILENO, filename);
        do_write(STDERR_FILENO, " because open(2) failed");
        do_write(STDERR_FILENO, strerror(original_errno));
        do_write(STDERR_FILENO, "\n");
    }
    return fd;
}


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
    if (dup2(old_fd, new_fd) == -1) {
        const int original_errno = errno;
        do_write(STDERR_FILENO, "dup2 failed: ");
        do_write(STDERR_FILENO, strerror(original_errno));
        do_write(STDERR_FILENO, "\n");
    }
}


/// Clears interrupts handling in a new child.
///
/// This must be invoked right after fork() to ensure the child process can
/// receive signals.
static void
reset_interrupts(void)
{
    const int ret = ::sigprocmask(SIG_SETMASK, &global_old_sigmask, NULL);
    assert(ret != -1);
}


/// Helper function for utils::process::child::fork_capture().
///
/// This function must be run immediately after calling fork() and can only use
/// async-safe functions.
///
/// \param hook The function to execute in the subprocess.  Must not return.
/// \param cookie Opaque data to pass to the hook.
void
utils_process_child_fork_capture(const void (*hook)(const void*),
                                 const void* cookie)
{
    reset_interrupts();
    setsid();

    close(fds[0]);
    safe_dup(fds[1], STDOUT_FILENO);
    safe_dup(fds[1], STDERR_FILENO);
    close(fds[1]);

    hook(cookie);

    do_write(STDERR_FILENO, "User-provided hook returned but it should "
             "not have");
    abort();
}


/// Helper function for utils::process::child::fork_files().
///
/// This function must be run immediately after calling fork() and can only use
/// async-safe functions.
///
/// \param hook The function to execute in the subprocess.  Must not return.
/// \param cookie Opaque data to pass to the hook.
/// \param stdout_file The name of the file in which to store the stdout.
/// \param stderr_file The name of the file in which to store the stderr.
void
utils_process_child_fork_files(const void (*hook)(const void*),
                               const void* cookie,
                               const char* stdout_file,
                               const char* stderr_file)
{
    reset_interrupts();
    setsid();

    if (strcmp(stdout_file, "/dev/stdout") != 0) {
        const int stdout_fd = open_for_append(stdout_file);
        safe_dup(stdout_fd, STDOUT_FILENO);
        close(stdout_fd);
    }
    if (strcmp(stderr_file, "/dev/stderr") != 0) {
        const int stderr_fd = open_for_append(stderr_file);
        safe_dup(stderr_fd, STDERR_FILENO);
        close(stderr_fd);
    }

    hook(cookie);

    do_write(STDERR_FILENO, "User-provided hook returned but it should "
             "not have");
    abort();
}
