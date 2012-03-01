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

#include "engine/atf_iface/runner.hpp"

extern "C" {
#include <signal.h>
#include <unistd.h>
}

#include <cerrno>
#include <fstream>

#include "engine/atf_iface/results.hpp"
#include "engine/atf_iface/test_case.hpp"
#include "engine/atf_iface/test_program.hpp"
#include "engine/exceptions.hpp"
#include "engine/isolation.ipp"
#include "engine/user_files/config.hpp"
#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"

namespace atf_iface = engine::atf_iface;
namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace process = utils::process;
namespace user_files = engine::user_files;

using utils::optional;


namespace {


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
can_do_unprivileged(const atf_iface::test_case& test_case,
                    const user_files::config& config)
{
    return test_case.required_user() == "unprivileged" &&
        config.unprivileged_user && passwd::current_user().is_root();
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


/// Functor to execute a test case's body in a subprocess.
class execute_test_case_body {
    /// Data of the test case to execute.
    atf_iface::test_case _test_case;

    /// Path to the results file to create.
    fs::path _result_file;

    /// Path to the work directory in which to run the test case.
    fs::path _work_directory;

    /// Parameters to configure the runtime environment of the test case.
    user_files::config _config;

    /// Exception-safe version of operator().
    void
    safe_run(void) const
    {
        const fs::path test_program = _test_case.test_program().absolute_path();
        const fs::path abs_test_program = test_program.is_absolute() ?
            test_program : test_program.to_absolute();

        engine::isolate_process(_work_directory);
        utils::setenv("__RUNNING_INSIDE_ATF_RUN", "internal-yes-value");

        if (can_do_unprivileged(_test_case, _config))
            passwd::drop_privileges(_config.unprivileged_user.get());

        std::vector< std::string > args;
        args.push_back(F("-r%s") % _result_file);
        args.push_back(F("-s%s") % abs_test_program.branch_path());
        config_to_args(_config, _test_case.test_program().test_suite_name(),
                       args);
        args.push_back(_test_case.name());
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
    execute_test_case_body(const atf_iface::test_case& test_case_,
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


/// Functor to execute a test case's cleanup routine in a subprocess.
class execute_test_case_cleanup {
    /// Data of the test case to execute.
    atf_iface::test_case _test_case;

    /// Path to the work directory in which to run the test case.
    fs::path _work_directory;

    /// Parameters to configure the runtime environment of the test case.
    user_files::config _config;

public:
    /// Constructor for the functor.
    ///
    /// \param test_case_ The data of the test case, including the path to the
    ///     test program that contains it, the test case name and its metadata.
    /// \param work_directory_ The path to the directory to chdir into when
    ///     running the test program.
    /// \param config_ The values for the current engine configuration.
    execute_test_case_cleanup(const atf_iface::test_case& test_case_,
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

        engine::isolate_process(_work_directory);
        utils::setenv("__RUNNING_INSIDE_ATF_RUN", "internal-yes-value");

        if (can_do_unprivileged(_test_case, _config))
            passwd::drop_privileges(_config.unprivileged_user.get());

        std::vector< std::string > args;
        args.push_back(F("-s%s") % abs_test_program.branch_path());
        config_to_args(_config, _test_case.test_program().test_suite_name(),
                       args);
        args.push_back(F("%s:cleanup") % _test_case.name());
        process::exec(abs_test_program, args);
    }
};


/// Functor to run a test case inside a controlled environment.
class run_test_case_safe {
    /// Data of the test case to debug.
    const atf_iface::test_case& _test_case;

    /// Parameters to configure the runtime environment of the test case.
    const user_files::config& _config;

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
    /// \param config_ The values for the current engine configuration.
    /// \param hooks_ Hooks to introspect the execution of the test case.
    /// \param stdout_path_ The file into which to store the test case's stdout.
    ///     If none, use a temporary file within the work directory.
    /// \param stderr_path_ The file into which to store the test case's stderr.
    ///     If none, use a temporary file within the work directory.
    run_test_case_safe(const atf_iface::test_case& test_case_,
                       const user_files::config& config_,
                       engine::test_case_hooks& hooks_,
                       const optional< fs::path >& stdout_path_,
                       const optional< fs::path >& stderr_path_) :
        _test_case(test_case_),
        _config(config_),
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

        if (can_do_unprivileged(_test_case, _config)) {
            set_owner(workdir, _config.unprivileged_user.get());
            set_owner(rundir, _config.unprivileged_user.get());
        }

        const fs::path result_file = workdir / "__RESULT__";
        const fs::path stdout_file =
            _stdout_path.get_default(workdir / "__STDOUT__");
        const fs::path stderr_file =
            _stderr_path.get_default(workdir / "__STDERR__");

        engine::check_interrupt();

        LI(F("Running test case body of '%s'") % _test_case.name());
        optional< process::status > body_status;
        try {
            body_status = engine::fork_and_wait(
                execute_test_case_body(_test_case, result_file, rundir, _config),
                stdout_file, stderr_file, _test_case.timeout());
        } catch (const engine::interrupted_error& e) {
            // Ignore: we want to attempt to run the cleanup function before we
            // return.  The call below to check_interrupt will reraise this
            // signal when it is safe to do so.
        }

        optional< process::status > cleanup_status;
        if (_test_case.has_cleanup()) {
            LI(F("Running test case cleanup of '%s'") % _test_case.name());
            cleanup_status = engine::fork_and_wait(
                execute_test_case_cleanup(_test_case, rundir, _config),
                stdout_file, stderr_file, _test_case.timeout());
        } else {
            cleanup_status = process::status::fake_exited(EXIT_SUCCESS);
        }

        _hooks.got_stdout(stdout_file);
        _hooks.got_stderr(stderr_file);

        engine::check_interrupt();

        return atf_iface::calculate_result(body_status, cleanup_status,
                                           result_file);
    }
};


}  // anonymous namespace


/// Runs a single test case in a controlled manner.
///
/// All exceptions raised at run time are captured and reported as a test
/// failure.  These exceptions may be really bugs in our code, but we do not
/// want them to crash the runtime system.
///
/// \param test_case The test to execute.
/// \param config The values for the current engine configuration.
/// \param stdout_path The file into which to store the test case's stdout.
///     If none, use a temporary file within the work directory.
/// \param stderr_path The file into which to store the test case's stderr.
///     If none, use a temporary file within the work directory.
///
/// \return The result of the test case execution.
///
/// \throw interrupted_error If the execution has been interrupted by the user.
engine::test_result
atf_iface::run_test_case(const atf_iface::test_case& test_case,
                         const user_files::config& config,
                         test_case_hooks& hooks,
                         const optional< fs::path >& stdout_path,
                         const optional< fs::path >& stderr_path)
{
    LI(F("Processing test case '%s'") % test_case.name());

    try {
        const std::string skip_reason = test_case.check_requirements(config);
        if (skip_reason.empty())
            return engine::protected_run(run_test_case_safe(
                test_case, config, hooks, stdout_path, stderr_path));
        else
            return engine::test_result(engine::test_result::skipped,
                                       skip_reason);
    } catch (const interrupted_error& e) {
        throw e;
    } catch (const std::exception& e) {
        return engine::test_result(engine::test_result::broken,
            F("The test caused an error in the runtime system: %s") % e.what());
    }
}
