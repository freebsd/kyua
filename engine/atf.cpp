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

#include "engine/atf.hpp"

extern "C" {
#include <sys/types.h>
#include <sys/wait.h>

#include <unistd.h>
}

#include <cstdio>
#include <cstdlib>
#include <fstream>

#include "engine/atf_result.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "utils/defs.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/optional.ipp"
#include "utils/process/operations.hpp"
#include "utils/process/status.hpp"

namespace config = utils::config;
namespace fs = utils::fs;
namespace process = utils::process;

using utils::none;
using utils::optional;


namespace {


/// Basename of the file containing the result written by the ATF test case.
static const char* result_name = "result.body";


/// Magic number returned by exec_test when the test case had a cleanup routine.
///
/// This is used by compute_result to know where to find the actual result of
/// the test case's body and cleanup routines because, in those rare cases when
/// the ATF test case has a cleanup routine, we have to do an extra dance here
/// to run it.  Note that this magic code prevents the ATF test case from ever
/// returning this number successfully -- but doing so would not be part of the
/// ATF interface and the test would be considered broken anyway.
static const int exit_with_cleanup = 108;


/// Basename of the file with the body exit status when the test has cleanup.
static const char* body_exit_cookie = "exit.body";


/// Basename of the file with the cleanup exit status when the test has cleanup.
static const char* cleanup_exit_cookie = "exit.cleanup";


/// Reads the exit status of a process from a file.
///
/// \param file The file to read from.  Must have been written by
///     write_exit_cookie().
///
/// \return The read status code if successful, or none otherwise.  The none
/// case most likely represents that the test case timed out halfway through and
/// was killed.
static optional< process::status >
read_exit_cookie(const fs::path& file)
{
    std::ifstream input(file.c_str());
    if (!input) {
        LD(F("No exit cookie %s: assuming timeout") % file);
        return none;
    }

    int status;
    input >> status;
    input.close();

    LD(F("Loaded exit cookie %s") % file);
    return utils::make_optional(utils::process::status(-1, status));
}


/// Writes the exit status of a process into a file.
///
/// This function is intended to be called from exec_test exclusively, as it
/// abruptly terminates the process when an error occurs.
///
/// \param status The exit status to write.
/// \param file The file to write to.
static void
write_exit_cookie(const int status, const fs::path& file)
{
    std::ofstream output(file.c_str());
    if (!output) {
        std::perror((F("Failed to open %s for write") % file).str().c_str());
        std::abort();
    }
    output << status;
    output.close();
}


/// Executes a test case part and records its exit status.
///
/// This function is intended to be called from exec_test exclusively, as it
/// abruptly terminates the process when an error occurs.
///
/// \param test_program Path to the test program to run.
/// \param args Arguments to pass to the test program.
/// \param exit_cookie The file to write to.
static void
run_part(const fs::path& test_program, const process::args_vector& args,
         const fs::path& exit_cookie)
{
    const pid_t pid = ::fork();
    if (pid == -1) {
        std::perror("fork(2) failed to run test case part");
        std::abort();
    } else if (pid == 0) {
        process::exec(test_program, args);
    }

    int status;
    if (::waitpid(pid, &status, 0) == -1) {
        std::perror("waitpid(2) failed to wait for test case part");
        std::abort();
    }
    write_exit_cookie(status, exit_cookie);
}


}  // anonymous namespace


/// Executes a test case of the test program.
///
/// This method is intended to be called within a subprocess and is expected
/// to terminate execution either by exec(2)ing the test program or by
/// exiting with a failure.
///
/// \param test_program The test program to execute.
/// \param test_case_name Name of the test case to invoke.
/// \param vars User-provided variables to pass to the test program.
/// \param control_directory Directory where the interface may place control
///     files.
void
engine::atf_interface::exec_test(const model::test_program& test_program,
                                 const std::string& test_case_name,
                                 const config::properties_map& vars,
                                 const fs::path& control_directory) const
{
    utils::setenv("__RUNNING_INSIDE_ATF_RUN", "internal-yes-value");

    const model::test_case& test_case = test_program.find(test_case_name);
    const bool has_cleanup = test_case.get_metadata().has_cleanup();

    process::args_vector args;
    for (config::properties_map::const_iterator iter = vars.begin();
         iter != vars.end(); ++iter) {
        args.push_back(F("-v%s=%s") % (*iter).first % (*iter).second);
    }

    if (!has_cleanup) {
        args.push_back(F("-r%s") % (control_directory / result_name));
        args.push_back(test_case_name);
        process::exec(test_program.absolute_path(), args);
    } else {
        process::args_vector body_args = args;
        body_args.push_back(F("-r%s") % (control_directory / result_name));
        body_args.push_back(F("%s:body") % test_case_name);
        run_part(test_program.absolute_path(), body_args,
                 control_directory / body_exit_cookie);

        process::args_vector cleanup_args = args;
        cleanup_args.push_back(F("%s:cleanup") % test_case_name);
        run_part(test_program.absolute_path(), cleanup_args,
                 control_directory / cleanup_exit_cookie);

        std::exit(exit_with_cleanup);
    }
}


/// Computes the result of a test case based on its termination status.
///
/// \param status The termination status of the subprocess used to execute
///     the exec_test() method or none if the test timed out.
/// \param control_directory Directory where the interface may have placed
///     control files.
/// \param unused_stdout_path Path to the file containing the stdout of the
///     test.
/// \param unused_stderr_path Path to the file containing the stderr of the
///     test.
///
/// \return A test result.
model::test_result
engine::atf_interface::compute_result(
    const optional< process::status >& status,
    const fs::path& control_directory,
    const fs::path& UTILS_UNUSED_PARAM(stdout_path),
    const fs::path& UTILS_UNUSED_PARAM(stderr_path)) const
{
    if (!status || (status.get().exited() &&
                    status.get().exitstatus() == exit_with_cleanup)) {
        // This is the slow and uncommon case.  The test case either timed out
        // or had a standalone cleanup routine and we had to run it; we do not
        // know which it is, but it does not matter much.  Because the scheduler
        // interface only wants to see a single subprocess (for good reason), we
        // handle here our internal spawning of two processes by loading their
        // results from disk.
        LD("Loading ATF test case result from on-disk exit cookies");

        const optional< process::status > body_status =
            read_exit_cookie(control_directory / body_exit_cookie);

        optional< process::status > cleanup_status =
            read_exit_cookie(control_directory / cleanup_exit_cookie);
        if (!body_status && !cleanup_status) {
            // Currently, this implementation of the ATF interface is unable to
            // execute the cleanup routine after the body of a test has timed
            // out.  If we detect that the body timed out, then we fake the exit
            // status of the cleanup routine to not confuse
            // calculate_atf_result(); otherwise, expected timeouts would not
            // work.
            // TODO(jmmv): This is obviously a hack to cope with our incomplete
            // implementation of the ATF interface and we need to fix that.
            cleanup_status = utils::process::status::fake_exited(EXIT_SUCCESS);
        }

        return calculate_atf_result(body_status, cleanup_status,
                                    control_directory / result_name);
    } else {
        // This is the fast and common case.  The test case had no standalone
        // cleanup routine so we just fake its exit code when computing the
        // result.
        const process::status cleanup_status =
            utils::process::status::fake_exited(EXIT_SUCCESS);
        return calculate_atf_result(status,
                                    utils::make_optional(cleanup_status),
                                    control_directory / result_name);
    }
}
