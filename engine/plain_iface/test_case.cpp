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

extern "C" {
#include <signal.h>
}

#include <cerrno>
#include <iostream>

#include "engine/exceptions.hpp"
#include "engine/isolation.ipp"
#include "engine/plain_iface/test_case.hpp"
#include "engine/plain_iface/test_program.hpp"
#include "engine/results.hpp"
#include "utils/defs.hpp"
#include "utils/fs/operations.hpp"
#include "utils/optional.ipp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace plain_iface = engine::plain_iface;
namespace process = utils::process;
namespace results = engine::results;

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
        return F("Exited with code %d") % status.exitstatus();
    else if (status.signaled())
        return F("Received signal %d%s") % status.termsig() %
            (status.coredump() ? " (core dumped)" : "");
    else
        return F("Terminated in an unknown manner");
}


/// Functor to execute a test case in a subprocess.
class execute_test_case {
    plain_iface::test_case _test_case;
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
static results::result_ptr
calculate_result(const optional< process::status >& maybe_status)
{
    if (!maybe_status)
        return results::result_ptr(new results::broken("Test case timed out"));
    const process::status& status = maybe_status.get();

    if (status.exited()) {
        if (status.exitstatus() == EXIT_SUCCESS)
            return results::result_ptr(new results::passed());
        else if (status.exitstatus() == exec_failure_code)
            return results::result_ptr(
                new results::broken("Failed to execute test program"));
        else
            return results::result_ptr(
                new results::failed(format_status(status)));
    } else {
        return results::result_ptr(new results::broken(
                                       format_status(status)));
    }
}


class run_test_case_safe {
    const plain_iface::test_case& _test_case;

public:
    run_test_case_safe(const plain_iface::test_case& test_case_) :
        _test_case(test_case_)
    {
    }

    /// Auxiliary function to execute a test case within a work directory.
    ///
    /// This is an auxiliary function for run_test_case_safe that is protected from
    /// the reception of common termination signals.
    ///
    /// \param test_case The test case to execute.
    /// \param workdir The directory in which the test case has to be run.
    ///
    /// \return The result of the execution of the test case.
    ///
    /// \throw interrupted_error If the execution has been interrupted by the user.
    results::result_ptr
    operator()(const fs::path& workdir) const
    {
        const fs::path rundir(workdir / "run");
        fs::mkdir(rundir, 0755);

        engine::check_interrupt();

        const plain_iface::test_program* test_program =
            dynamic_cast< const plain_iface::test_program* >(
                &_test_case.test_program());

        LI(F("Running test case '%s'") % _test_case.identifier().str());
        optional< process::status > body_status = engine::fork_and_wait(
            execute_test_case(_test_case, rundir),
            workdir / "stdout.txt", workdir / "stderr.txt",
            test_program->timeout());

        engine::check_interrupt();

        return calculate_result(body_status);
    }
};


}  // anonymous namespace


/// Constructs a new test case.
///
/// \param test_program_ The test program this test case belongs to.  This
///     object must exist during the lifetime of the test case.
plain_iface::test_case::test_case(const base_test_program& test_program_) :
    base_test_case(test_program_, "main")
{
}


/// Returns a string representation of all test case properties.
///
/// The returned keys and values match those that can be defined by the test
/// case.
///
/// \return A key/value mapping describing all the test case properties.
engine::properties_map
plain_iface::test_case::get_all_properties(void) const
{
    properties_map props;

    const plain_iface::test_program* plain_test_program =
        dynamic_cast< const plain_iface::test_program* >(&this->test_program());

    const datetime::delta& timeout = plain_test_program->timeout();
    if (timeout != detail::default_timeout) {
        INV(timeout.useconds == 0);
        props["timeout"] = F("%d") % timeout.seconds;
    }

    return props;
}


/// Executes the test case.
///
/// This should not throw any exception: problems detected during execution are
/// reported as a broken test case result.
///
/// \param unused_config The run-time configuration for the test case.
///
/// \return The result of the execution.
engine::results::result_ptr
plain_iface::test_case::do_run(
    const user_files::config& UTILS_UNUSED_PARAM(config)) const
{
    LI(F("Processing test case '%s'") % identifier().str());

    results::result_ptr result;
    try {
        result = engine::protected_run(run_test_case_safe(*this));
    } catch (const interrupted_error& e) {
        throw e;
    } catch (const std::exception& e) {
        result = results::result_ptr(new results::broken(F(
            "The test caused an error in the runtime system: %s") % e.what()));
    }
    INV(result.get() != NULL);
    return result;
}
