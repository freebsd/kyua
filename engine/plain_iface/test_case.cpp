// Copyright 2011 Google Inc.
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

#include "engine/plain_iface/test_case.hpp"

extern "C" {
#include <signal.h>
}

#include <cerrno>
#include <iostream>

#include "engine/exceptions.hpp"
#include "engine/isolation.ipp"
#include "engine/plain_iface/test_program.hpp"
#include "engine/test_result.hpp"
#include "utils/defs.hpp"
#include "utils/fs/operations.hpp"
#include "utils/optional.ipp"
#include "utils/stacktrace.hpp"

namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace plain_iface = engine::plain_iface;
namespace process = utils::process;

using utils::none;
using utils::optional;


namespace {


/// Exit code returned when the exec of the test program fails.
static int exec_failure_code = 120;


/// Formats the termination status of a process to be used with validate_result.
///
/// \param status The status to format.
///
/// \return A string describing the status.
static std::string
format_status(const process::status& status)
{
    if (status.exited())
        return F("Exited with code %s") % status.exitstatus();
    else if (status.signaled())
        return F("Received signal %s%s") % status.termsig() %
            (status.coredump() ? " (core dumped)" : "");
    else
        return F("Terminated in an unknown manner");
}


/// Functor to execute a test case in a subprocess.
class execute_test_case {
    /// Data of the test case to execute.
    plain_iface::test_case _test_case;

    /// Path to the work directory in which to run the test case.
    fs::path _work_directory;

    /// Exception-safe version of operator().
    void
    safe_run(void) const
    {
        const fs::path test_program = _test_case.test_program().absolute_path();
        const fs::path abs_test_program = test_program.is_absolute() ?
            test_program : test_program.to_absolute();

        engine::isolate_process(_work_directory);

        std::vector< std::string > args;
        try {
            process::exec(abs_test_program, args);
        } catch (const process::system_error& e) {
            std::cerr << "Failed to execute test program: " << e.what() << '\n';
            std::exit(exec_failure_code);
        }
    }

public:
    /// Constructor for the functor.
    ///
    /// \param test_case_ The data of the test case, including the program name,
    ///     the test case name and its metadata.
    /// \param work_directory_ The path to the directory to chdir into when
    ///     running the test program.
    execute_test_case(const plain_iface::test_case& test_case_,
                      const fs::path& work_directory_) :
        _test_case(test_case_),
        _work_directory(work_directory_)
    {
    }

    /// Entry point for the functor.
    void
    operator()(void)
    {
        try {
            safe_run();
        } catch (const std::runtime_error& e) {
            std::cerr << "Caught unhandled exception while setting up the test"
                "case: " << e.what() << '\n';
        } catch (...) {
            std::cerr << "Caught unknown exception while setting up the test"
                "case\n";
        }
        std::abort();
    }
};


/// Converts the exit status of the test program to a result.
///
/// \param maybe_status The exit status of the program, or none if it timed out.
///
/// \return A test case result.
static engine::test_result
calculate_result(const optional< process::status >& maybe_status)
{
    using engine::test_result;

    if (!maybe_status)
        return test_result(test_result::broken, "Test case timed out");
    const process::status& status = maybe_status.get();

    if (status.exited()) {
        if (status.exitstatus() == EXIT_SUCCESS)
            return test_result(test_result::passed);
        else if (status.exitstatus() == exec_failure_code)
            return test_result(test_result::broken,
                               "Failed to execute test program");
        else
            return test_result(test_result::failed, format_status(status));
    } else {
        return test_result(test_result::broken, format_status(status));
    }
}


/// Functor to execute the test case.
class run_test_case_safe {
    /// Data of the test case to debug.
    const plain_iface::test_case& _test_case;

    /// Hooks to introspect the execution of the test case.
    engine::test_case_hooks& _hooks;

    /// The file into which to store the test case's stdout.  If none, use a
    /// temporary file within the work directory.
    const optional< fs::path > _stdout_path;

    /// The file into which to store the test case's stderr.  If none, use a
    /// temporary file within the work directory.
    const optional< fs::path > _stderr_path;

public:
    /// Constructor for the functor.
    ///
    /// \param test_case_ The data of the test case, including the path to the
    ///     test program that contains it, the test case name and its metadata.
    /// \param hooks_ Hooks to introspect the execution of the test case.
    /// \param stdout_path_ The file into which to store the test case's stdout.
    ///     If none, use a temporary file within the work directory.
    /// \param stderr_path_ The file into which to store the test case's stderr.
    ///     If none, use a temporary file within the work directory.
    run_test_case_safe(const plain_iface::test_case& test_case_,
                       engine::test_case_hooks& hooks_,
                       const optional< fs::path >& stdout_path_,
                       const optional< fs::path >& stderr_path_) :
        _test_case(test_case_),
        _hooks(hooks_),
        _stdout_path(stdout_path_),
        _stderr_path(stderr_path_)
    {
    }

    /// Auxiliary function to execute a test case within a work directory.
    ///
    /// This is an auxiliary function for run_test_case_safe that is protected
    /// from the reception of common termination signals.
    ///
    /// \param workdir The directory in which the test case has to be run.
    ///
    /// \return The result of the execution of the test case.
    ///
    /// \throw interrupted_error If the execution has been interrupted by the
    /// user.
    engine::test_result
    operator()(const fs::path& workdir) const
    {
        const fs::path rundir(workdir / "run");
        fs::mkdir(rundir, 0755);

        engine::check_interrupt();

        const plain_iface::test_program* test_program =
            dynamic_cast< const plain_iface::test_program* >(
                &_test_case.test_program());

        const fs::path stdout_file =
            _stdout_path.get_default(workdir / "stdout.txt");
        const fs::path stderr_file =
            _stderr_path.get_default(workdir / "stderr.txt");

        LI(F("Running test case '%s'") % _test_case.name());
        optional< process::status > body_status = engine::fork_and_wait(
            execute_test_case(_test_case, rundir), stdout_file, stderr_file,
            test_program->timeout());
        utils::dump_stacktrace_if_available(
            _test_case.test_program().absolute_path(), body_status, rundir,
            stderr_file);

        engine::check_interrupt();

        _hooks.got_stdout(stdout_file);
        _hooks.got_stderr(stderr_file);

        return calculate_result(body_status);
    }
};


/// Executes the test case.
///
/// This should not throw any exception: problems detected during execution are
/// reported as a broken test case result.
///
/// \param test_case The test case to debug or run.
/// \param unused_user_config The run-time configuration for the test case.
/// \param hooks Hooks to introspect the execution of the test case.
/// \param stdout_path The file into which to store the test case's stdout.
///     If none, use a temporary file within the work directory.
/// \param stderr_path The file into which to store the test case's stderr.
///     If none, use a temporary file within the work directory.
///
/// \return The result of the execution.
static engine::test_result
execute(const engine::base_test_case* test_case,
        const config::tree& UTILS_UNUSED_PARAM(user_config),
        engine::test_case_hooks& hooks,
        const optional< fs::path >& stdout_path,
        const optional< fs::path >& stderr_path)
{
    LI(F("Processing test case '%s'") % test_case->name());

    try {
        const engine::plain_iface::test_case* tc =
            dynamic_cast< const engine::plain_iface::test_case* >(test_case);
        return engine::protected_run(run_test_case_safe(
            *tc, hooks, stdout_path, stderr_path));
    } catch (const engine::interrupted_error& e) {
        throw e;
    } catch (const std::exception& e) {
        return engine::test_result(engine::test_result::broken, F(
            "The test caused an error in the runtime system: %s") % e.what());
    }
}


/// Constructs a metadata object for the single test case in the test program.
///
/// \param test_program_ Reference to the generic test program.
///
/// \return A metadata object.
static engine::metadata
new_metadata(const engine::base_test_program& test_program_)
{
    const plain_iface::test_program& plain_test_program =
        dynamic_cast< const plain_iface::test_program& >(test_program_);
    return engine::metadata_builder()
        .set_timeout(plain_test_program.timeout())
        .build();
}


}  // anonymous namespace


/// Constructs a new test case.
///
/// \param test_program_ The test program this test case belongs to.  This
///     object must exist during the lifetime of the test case.
plain_iface::test_case::test_case(const base_test_program& test_program_) :
    base_test_case("plain", test_program_, "main", new_metadata(test_program_))
{
}


/// Runs the test case in debug mode.
///
/// Debug mode gives the caller more control on the execution of the test.  It
/// should not be used for normal execution of tests; instead, call run().
///
/// \param test_case The test case to debug.
/// \param user_config The user configuration that defines the execution of this
///     test case.
/// \param hooks Hooks to introspect the execution of the test case.
/// \param stdout_path The file to which to redirect the stdout of the test.
///     For interactive debugging, '/dev/stdout' is probably a reasonable value.
/// \param stderr_path The file to which to redirect the stdout of the test.
///     For interactive debugging, '/dev/stderr' is probably a reasonable value.
///
/// \return The result of the execution of the test case.
engine::test_result
engine::plain_iface::debug_plain_test_case(const base_test_case* test_case,
                                           const config::tree& user_config,
                                           test_case_hooks& hooks,
                                           const fs::path& stdout_path,
                                           const fs::path& stderr_path)
{
    return execute(test_case, user_config, hooks,
                   utils::make_optional(stdout_path),
                   utils::make_optional(stderr_path));
}


/// Runs the test case.
///
/// \param test_case The test case to run.
/// \param user_config The user configuration that defines the execution of this
///     test case.
/// \param hooks Hooks to introspect the execution of the test case.
///
/// \return The result of the execution of the test case.
engine::test_result
engine::plain_iface::run_plain_test_case(const base_test_case* test_case,
                                         const config::tree& user_config,
                                         test_case_hooks& hooks)
{
    return execute(test_case, user_config, hooks, none, none);
}
