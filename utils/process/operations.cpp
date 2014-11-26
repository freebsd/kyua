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

#include <unistd.h>
}

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/process/exceptions.hpp"
#include "utils/process/system.hpp"
#include "utils/process/status.hpp"
#include "utils/signals/interrupts.hpp"

namespace fs = utils::fs;
namespace process = utils::process;
namespace signals = utils::signals;


/// Maximum number of arguments supported by exec.
///
/// We need this limit to avoid having to allocate dynamic memory in the child
/// process to construct the arguments list, which would have side-effects in
/// the parent's memory if we use vfork().
#define MAX_ARGS 128


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


/// Executes an external binary and replaces the current process.
///
/// This function must not use any of the logging features so that the output
/// of the subprocess is not "polluted" by our own messages.
///
/// This function must also not affect the global state of the current process
/// as otherwise we would not be able to use vfork().  Only state stored in the
/// stack can be touched.
///
/// \param program The binary to execute.
/// \param args The arguments to pass to the binary, without the program name.
void
process::exec(const fs::path& program, const args_vector& args) throw()
{
    assert(args.size() < MAX_ARGS);
    try {
        const char* argv[MAX_ARGS + 1];

        argv[0] = program.c_str();
        for (args_vector::size_type i = 0; i < args.size(); i++)
            argv[1 + i] = args[i].c_str();
        argv[1 + args.size()] = NULL;

        const int ret = ::execv(program.c_str(),
                                (char* const*)(unsigned long)(const void*)argv);
        const int original_errno = errno;
        assert(ret == -1);

        std::cerr << "Failed to execute " << program << ": "
                  << std::strerror(original_errno) << "\n";
        std::abort();
    } catch (const std::runtime_error& error) {
        std::cerr << "Failed to execute " << program << ": "
                  << error.what() << "\n";
        std::abort();
    } catch (...) {
        std::cerr << "Failed to execute " << program << "; got unexpected "
            "exception during exec\n";
        std::abort();
    }
}


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
