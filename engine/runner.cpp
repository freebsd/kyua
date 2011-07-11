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
#include <sys/stat.h>

#include <signal.h>
#include <unistd.h>
}

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/atf_results.hpp"
#include "engine/atf_test_case.hpp"
#include "engine/exceptions.hpp"
#include "engine/results.ipp"
#include "engine/runner.hpp"
#include "engine/test_program.hpp"
#include "engine/user_files/config.hpp"
#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"
#include "utils/fs/auto_cleaners.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"
#include "utils/process/children.ipp"
#include "utils/process/exceptions.hpp"
#include "utils/sanity.hpp"
#include "utils/signals/exceptions.hpp"
#include "utils/signals/misc.hpp"
#include "utils/signals/programmer.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace process = utils::process;
namespace results = engine::results;
namespace runner = engine::runner;
namespace signals = utils::signals;
namespace user_files = engine::user_files;

using utils::none;
using utils::optional;


namespace {


/// Number of the stop signal.
///
/// This is set by interrupt_handler() when it receives a signal that ought to
/// terminate the execution of the current test case.
static int interrupted_signo = 0;


/// Signal handler for termination signals.
///
/// \param signo The signal received.
///
/// \post interrupted_signo is set to the received signal.
static void
interrupt_handler(const int signo)
{
    const char* message = "[-- Signal caught; please wait for clean up --]\n";
    ::write(STDERR_FILENO, message, std::strlen(message));
    interrupted_signo = signo;

    POST(interrupted_signo != 0);
    POST(interrupted_signo == signo);
}


/// Syntactic sugar to validate if there is a pending signal.
///
/// \throw interrupted_error If there is a pending signal that ought to
///     terminate the execution of the program.
static void
check_interrupt(void)
{
    LD("Checking for pending interrupt signals");
    if (interrupted_signo != 0) {
        LI("Interrupt pending; raising error to cause cleanup");
        throw engine::interrupted_error(interrupted_signo);
    }
}


/// Atomically creates a new work directory with a unique name.
///
/// The directory is created under the system-wide configured temporary
/// directory as defined by the TMPDIR environment variable.
///
/// \return The path to the new work directory.
///
/// \throw fs::error If there is a problem creating the temporary directory.
static fs::path
create_work_directory(void)
{
    const char* tmpdir = std::getenv("TMPDIR");
    if (tmpdir == NULL)
        return fs::mkdtemp(fs::path("/tmp/kyua.XXXXXX"));
    else
        return fs::mkdtemp(fs::path(F("%s/kyua.XXXXXX") % tmpdir));
}


/// Sets the owner of a file or directory.
///
/// \param path The file to affect.
/// \param owner The new owner for the file.
///
/// \throw fs::system_error If there is a problem changing the ownership.
static void
set_owner(const fs::path& path, const passwd::user& owner)
{
    if (::chown(path.c_str(), owner.uid, owner.gid) == -1)
        throw std::runtime_error(F("Failed to set owner of %s to %s") %
                                 path % owner.name);
}


/// Check if we can (and should) drop privileges for a test case.
///
/// \param test_case The test case to be run.  Needed to inspect its
///     required_user property.
/// \param config The current configuration.  Needed to query if
///     unprivileged_user is defined or not.
///
/// \return True if we can drop privileges; false otherwise.
static bool
can_do_unprivileged(const engine::atf_test_case& test_case,
                    const user_files::config& config)
{
    return test_case.required_user == "unprivileged" &&
        config.unprivileged_user && passwd::current_user().is_root();
}


/// Isolates the current process from the rest of the system.
///
/// This is intended to be used right before executing a test program because it
/// attempts to isolate the current process from the rest of the system.
///
/// By isolation, we understand:
///
/// * Change the cwd of the process to a known location that will be cleaned up
///   afterwards by the runner monitor.
/// * Reset a set of critical environment variables to known good values.
/// * Reset the umask to a known value.
/// * Reset the signal handlers.
///
/// \throw std::runtime_error If there is a problem setting up the process
///     environment.
static void
isolate_process(const fs::path& cwd)
{
    // The utils::process library takes care of creating a process group for
    // us.  Just ensure that is still true, or otherwise things will go pretty
    // badly.
    INV(::getpgrp() == ::getpid());

    ::umask(0022);

    for (int i = 0; i <= signals::last_signo; i++) {
        try {
            if (i != SIGKILL && i != SIGSTOP)
                signals::reset(i);
        } catch (const signals::system_error& e) {
            // Just ignore errors trying to reset signals.  It might happen
            // that we try to reset an immutable signal that we are not aware
            // of, so we certainly do not want to make a big deal of it.
        }
    }

    // TODO(jmmv): It might be better to do the opposite: just pass a good known
    // set of variables to the child (aka HOME, PATH, ...).  But how do we
    // determine this minimum set?
    utils::unsetenv("LANG");
    utils::unsetenv("LC_ALL");
    utils::unsetenv("LC_COLLATE");
    utils::unsetenv("LC_CTYPE");
    utils::unsetenv("LC_MESSAGES");
    utils::unsetenv("LC_MONETARY");
    utils::unsetenv("LC_NUMERIC");
    utils::unsetenv("LC_TIME");

    utils::setenv("TZ", "UTC");

    utils::setenv("__RUNNING_INSIDE_ATF_RUN", "internal-yes-value");

    if (::chdir(cwd.c_str()) == -1)
        throw std::runtime_error(F("Failed to enter work directory %s") % cwd);
    utils::setenv("HOME", fs::current_path().str());
}


/// Converts a set of configuration variables to test program flags.
///
/// \param config The configuration variables provided by the user.
/// \param test_suite The name of the test suite.
/// \param args [out] The test program arguments in which to add the new flags.
static void
config_to_args(const user_files::config& config,
               const std::string& test_suite,
               std::vector< std::string >& args)
{
    if (config.unprivileged_user)
        args.push_back(F("-vunprivileged-user=%s") %
                       config.unprivileged_user.get().name);

    const user_files::properties_map& properties = config.test_suite(
        test_suite);
    for (user_files::properties_map::const_iterator iter = properties.begin();
         iter != properties.end(); iter++) {
        args.push_back(F("-v%s=%s") % (*iter).first % (*iter).second);
    }
}


static void report_broken_result(const fs::path&, const std::string&)
    UTILS_NORETURN;


/// Creates a 'broken' results file and exits.
///
/// \param result_file The location of the results file.
/// \param reason The reason for the breakage to report to the caller.
static void
report_broken_result(const fs::path& result_file, const std::string& reason)
{
    std::ofstream result(result_file.c_str());
    result << F("broken: %s\n") % reason;
    result.close();
    std::exit(EXIT_FAILURE);
}


/// Functor to execute a test case in a subprocess.
class execute_test_case_body {
    engine::atf_test_case _test_case;
    fs::path _result_file;
    fs::path _work_directory;
    user_files::config _config;

    /// Exception-safe version of operator().
    void
    safe_run(void) const
    {
        const fs::path test_program = _test_case.test_program().absolute_path();
        const fs::path abs_test_program = test_program.is_absolute() ?
            test_program : test_program.to_absolute();

        isolate_process(_work_directory);

        if (can_do_unprivileged(_test_case, _config))
            passwd::drop_privileges(_config.unprivileged_user.get());

        std::vector< std::string > args;
        args.push_back(F("-r%s") % _result_file);
        args.push_back(F("-s%s") % abs_test_program.branch_path());
        config_to_args(_config, _test_case.test_program().test_suite_name(),
                       args);
        args.push_back(_test_case.identifier().name);
        process::exec(abs_test_program, args);
    }

public:
    /// Constructor for the functor.
    ///
    /// \param test_case_ The data of the test case, including the program name,
    ///     the test case name and its metadata.
    /// \param result_file_ The path to the file in which to store the result of
    ///     the test case execution.
    /// \param work_directory_ The path to the directory to chdir into when
    ///     running the test program.
    /// \param config_ The configuration variables provided by the user.
    execute_test_case_body(const engine::atf_test_case& test_case_,
                           const fs::path& result_file_,
                           const fs::path& work_directory_,
                           const user_files::config& config_) :
        _test_case(test_case_),
        _result_file(result_file_),
        _work_directory(work_directory_),
        _config(config_)
    {
    }

    /// Entry point for the functor.
    void
    operator()(void)
    {
        try {
            safe_run();
        } catch (const std::runtime_error& e) {
            report_broken_result(_result_file, e.what());
        } catch (...) {
            report_broken_result(_result_file, "Caught unknown exception while "
                                 "setting up the test case");
        }
    }
};


/// Functor to execute a test case in a subprocess.
class execute_test_case_cleanup {
    engine::atf_test_case _test_case;
    fs::path _work_directory;
    user_files::config _config;

public:
    /// Constructor for the functor.
    ///
    /// \param test_case_ The data of the test case, including the path to the
    ///     test program that contains it, the test case name and its metadata.
    /// \param work_directory_ The path to the directory to chdir into when
    ///     running the test program.
    /// \param config_ The values for the current engine configuration.
    execute_test_case_cleanup(const engine::atf_test_case& test_case_,
                              const fs::path& work_directory_,
                              const user_files::config& config_) :
        _test_case(test_case_),
        _work_directory(work_directory_),
        _config(config_)
    {
    }

    /// Entry point for the functor.
    void
    operator()(void)
    {
        const fs::path test_program = _test_case.test_program().absolute_path();
        const fs::path abs_test_program = test_program.is_absolute() ?
            test_program : test_program.to_absolute();

        isolate_process(_work_directory);

        if (can_do_unprivileged(_test_case, _config))
            passwd::drop_privileges(_config.unprivileged_user.get());

        std::vector< std::string > args;
        args.push_back(F("-s%s") % abs_test_program.branch_path());
        config_to_args(_config, _test_case.test_program().test_suite_name(),
                       args);
        args.push_back(F("%s:cleanup") % _test_case.identifier().name);
        process::exec(abs_test_program, args);
    }
};


/// Forks a subprocess and waits for its completion.
///
/// \param hook The code to execute in the subprocess.
/// \param outfile The file that will receive the stdout output.
/// \param errfile The file that will receive the stderr output.
/// \param timeout The amount of time given to the subprocess to execute.
///
/// \return The exit status of the process or none if the timeout expired.
template< class Hook >
optional< process::status >
fork_and_wait(Hook hook, const fs::path& outfile, const fs::path& errfile,
              const datetime::delta& timeout)
{
    std::auto_ptr< process::child_with_files > child =
        process::child_with_files::fork(hook, outfile, errfile);
    try {
        return utils::make_optional(child->wait(timeout));
    } catch (const process::system_error& error) {
        if (error.original_errno() == EINTR) {
            (void)::kill(child->pid(), SIGKILL);
            (void)child->wait();
            check_interrupt();
            UNREACHABLE;
        } else
            throw error;
    } catch (const process::timeout_error& error) {
        return none;
    }
}


/// Auxiliary function to execute a test case within a work directory.
///
/// This is an auxiliary function for run_test_case_safe that is protected from
/// the reception of common termination signals.
///
/// \param test_case The test case to execute.
/// \param config The values for the current engine configuration.
/// \param workdir The directory in which the test case has to be run.
///
/// \return The result of the execution of the test case.
///
/// \throw interrupted_error If the execution has been interrupted by the user.
static results::result_ptr
run_test_case_safe_workdir(const engine::atf_test_case& test_case,
                           const user_files::config& config,
                           const fs::path& workdir)
{
    const fs::path rundir(workdir / "run");
    fs::mkdir(rundir, 0755);

    if (can_do_unprivileged(test_case, config)) {
        set_owner(workdir, config.unprivileged_user.get());
        set_owner(rundir, config.unprivileged_user.get());
    }

    const fs::path result_file(workdir / "result.txt");

    check_interrupt();

    LI(F("Running test case body for '%s'") % test_case.identifier().str());
    optional< process::status > body_status;
    try {
        body_status = fork_and_wait(
            execute_test_case_body(test_case, result_file, rundir, config),
            workdir / "stdout.txt", workdir / "stderr.txt", test_case.timeout);
    } catch (const engine::interrupted_error& e) {
        // Ignore: we want to attempt to run the cleanup function before we
        // return.  The call below to check_interrupt will reraise this signal
        // when it is safe to do so.
    }

    optional< process::status > cleanup_status;
    if (test_case.has_cleanup) {
        LI(F("Running test case cleanup for '%s'") %
           test_case.identifier().str());
        cleanup_status = fork_and_wait(
            execute_test_case_cleanup(test_case, rundir, config),
            workdir / "cleanup-stdout.txt", workdir / "cleanup-stderr.txt",
            test_case.timeout);
    }

    check_interrupt();

    return results::adjust(test_case, body_status, cleanup_status,
                           results::load(result_file));
}


/// Auxiliary function to execute a test case.
///
/// This is an auxiliary function for run_test_case that is protected from
/// leaking exceptions.  Any exception not managed here is probably a mistake,
/// but is correctly captured in the caller.
///
/// \param test_case The test case to execute.
/// \param config The values for the current engine configuration.
///
/// \return The result of the execution of the test case.
///
/// \throw interrupted_error If the execution has been interrupted by the user.
static results::result_ptr
run_test_case_safe(const engine::atf_test_case& test_case,
                   const user_files::config& config)
{
    const std::string skip_reason = test_case.check_requirements(config);
    if (!skip_reason.empty())
        return results::make_result(results::skipped(skip_reason));

    // These three separate objects are ugly.  Maybe improve in some way.
    signals::programmer sighup(SIGHUP, interrupt_handler);
    signals::programmer sigint(SIGINT, interrupt_handler);
    signals::programmer sigterm(SIGTERM, interrupt_handler);

    results::result_ptr result;

    fs::auto_directory workdir(create_work_directory());
    try {
        check_interrupt();
        result = run_test_case_safe_workdir(test_case, config,
                                            workdir.directory());

        try {
            workdir.cleanup();
        } catch (const fs::error& e) {
            if (result->good()) {
                result = results::make_result(results::broken(F(
                    "Could not clean up test work directory: %s") % e.what()));
            } else {
                LW(F("Not reporting work directory clean up failure because "
                     "the test is already broken: %s") % e.what());
            }
        }
    } catch (const engine::interrupted_error& e) {
        workdir.cleanup();

        sighup.unprogram();
        sigint.unprogram();
        sigterm.unprogram();

        throw e;
    }

    sighup.unprogram();
    sigint.unprogram();
    sigterm.unprogram();

    check_interrupt();

    return result;
}


}  // anonymous namespace


/// Runs a single test case in a controlled manner.
///
/// All exceptions raised at run time are captured and reported as a test
/// failure.  These exceptions may be really bugs in our code, but we do not
/// want them to crash the runtime system.
///
/// \param test_case The test to execute.
/// \param config The values for the current engine configuration.
///
/// \return The result of the test case execution.
///
/// \throw interrupted_error If the execution has been interrupted by the user.
results::result_ptr
runner::run_test_case(const engine::atf_test_case& test_case,
                      const user_files::config& config)
{
    LI(F("Processing test case '%s'") % test_case.identifier().str());

    results::result_ptr result;
    try {
        result = run_test_case_safe(test_case, config);
    } catch (const interrupted_error& e) {
        throw e;
    } catch (const std::exception& e) {
        result = results::make_result(results::broken(F(
            "The test caused an error in the runtime system: %s") % e.what()));
    }
    INV(result.get() != NULL);
    return result;
}
