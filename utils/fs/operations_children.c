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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils/defs.hpp"
#include "utils/fs/operations_children.h"


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


/// Helper function to execute mount.
///
/// This function must be run immediately after calling fork() and can only use
/// async-safe functions.
///
/// \param args NULL-terminated collection of arguments, including the program
///     name.
/// \param mount_point Location where the file system is being mounted; for
///     debugging purposes only.
void
utils_fs_operations_run_mount_tmpfs(const char* args[], const char* mount_point)
{
    const char** arg;
    do_write(STDOUT_FILENO, "Mounting tmpfs onto ");
    do_write(STDOUT_FILENO, mount_point);
    do_write(STDOUT_FILENO, " with:");
    for (arg = &args[0]; *arg != NULL; arg++) {
        do_write(STDOUT_FILENO, " ");
        do_write(STDOUT_FILENO, *arg);
    }
    do_write(STDOUT_FILENO, "\n");

    const int ret = execvp(args[0], UTILS_UNCONST(char* const, args));
    assert(ret == -1);
    do_write(STDERR_FILENO, "Failed to exec ");
    do_write(STDERR_FILENO, args[0]);
    do_write(STDERR_FILENO, "\n");
    exit(EXIT_FAILURE);
}


/// Helper function to execute unmount.
///
/// This function must be run immediately after calling fork() and can only use
/// async-safe functions.
///
/// \param unmount Name of the umount(8) binary.
/// \param mount_point Path to unmount.
void
utils_fs_operations_run_unmount(const char* unmount, const char* mount_point)
{
    const int ret = execlp(unmount, "umount", mount_point, NULL);
    assert(ret == -1);
    do_write(STDERR_FILENO, "Failed to exec ");
    do_write(STDERR_FILENO, unmount);
    do_write(STDERR_FILENO, "\n");
    exit(EXIT_FAILURE);
}
