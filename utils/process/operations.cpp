// Copyright 2014 Google Inc.
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

#include "utils/process/operations.hpp"

extern "C" {
#include <sys/types.h>
#include <sys/wait.h>
}

#include <cerrno>

#include "utils/logging/macros.hpp"
#include "utils/process/exceptions.hpp"
#include "utils/process/system.hpp"
#include "utils/process/status.hpp"
#include "utils/signals/interrupts.hpp"

namespace process = utils::process;
namespace signals = utils::signals;


namespace {


/// Exception-based, type-improved version of wait(2).
///
/// \return The PID of the terminated process and its termination status.
///
/// \throw process::system_error If the call to wait(2) fails.
static process::status
safe_wait(void)
{
    LD("Waiting for any child process");
    int stat_loc;
    const pid_t pid = ::wait(&stat_loc);
    if (pid == -1) {
        const int original_errno = errno;
        throw process::system_error("Failed to wait for any child process",
                                    original_errno);
    }
    return process::status(pid, stat_loc);
}


}  // anonymous namespace


/// Blocks to wait for completion of any subprocess.
///
/// \return The termination status of the child process that terminated.
///
/// \throw process::system_error If the call to wait(2) fails.
process::status
process::wait_any(void)
{
    const process::status status = safe_wait();
    {
        signals::interrupts_inhibiter inhibiter;
        signals::remove_pid_to_kill(status.dead_pid());
    }
    return status;
}
