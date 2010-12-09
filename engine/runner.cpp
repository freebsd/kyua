// Copyright 2010, Google Inc.
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

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/results.ipp"
#include "engine/runner.hpp"
#include "engine/suite_config.hpp"
#include "engine/test_case.hpp"
#include "engine/test_program.hpp"
#include "utils/defs.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/auto_cleaners.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/process/children.ipp"
#include "utils/sanity.hpp"
#include "utils/signals/exceptions.hpp"
#include "utils/signals/misc.hpp"

namespace fs = utils::fs;
namespace process = utils::process;
namespace results = engine::results;
namespace runner = engine::runner;
namespace signals = utils::signals;


namespace {


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


/// Isolates the current process from the rest of the system.
///
/// This is intended to be used right before executing a test program because it
/// attempts to isolate the current process from the rest of the system.
///
/// By isolation, we understand:
///
/// * Create a new process group.
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
    const int ret = ::setpgid(::getpid(), 0);
    INV(ret != -1);

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
    utils::unsetenv("TZ");

    if (::chdir(cwd.c_str()) == -1)
        throw std::runtime_error(F("Failed to enter work directory %s") % cwd);
    utils::setenv("HOME", fs::current_path().str());
}


/// Converts a set of configuration variables to test program flags.
///
/// \param config The configuration variables.
/// \param args [out] The test program arguments in which to add the new flags.
static void
config_to_args(const engine::properties_map& config,
               std::vector< std::string >& args)
{
    for (engine::properties_map::const_iterator iter = config.begin();
         iter != config.end(); iter++) {
        args.push_back(F("-v%s=%s") % (*iter).first % (*iter).second);
    }
}


/// Functor to execute a test case in a subprocess.
class execute_test_case_body {
    engine::test_case _test_case;
    fs::path _result_file;
    fs::path _work_directory;
    engine::properties_map _config;

public:
    /// Constructor for the functor.
    ///
    /// \param test_case_ The data of the test case, including the path to the
    ///     test program that contains it, the test case name and its metadata.
    /// \param result_file_ The path to the file in which to store the result of
    ///     the test case execution.
    /// \param work_directory_ The path to the directory to chdir into when
    ///     running the test program.
    /// \param config_ The configuration variables provided by the user.
    execute_test_case_body(const engine::test_case& test_case_,
                           const fs::path& result_file_,
                           const fs::path& work_directory_,
                           const engine::properties_map& config_) :
        _test_case(test_case_),
        _result_file(result_file_),
        _work_directory(work_directory_),
        _config(config_)
    {
    }

    /// Entry point for the functor.
    void
    operator()(void) UTILS_NORETURN
    {
        const fs::path test_program = (
            _test_case.program().is_absolute() ? _test_case.program() :
            fs::current_path() / _test_case.program());

        try {
            isolate_process(_work_directory);
        } catch (const std::exception& error) {
            std::cerr << F("Failed to set up test case: %s\n") % error.what();
            std::abort();
        }

        std::vector< std::string > args;
        args.push_back(F("-r%s") % _result_file);
        args.push_back(F("-s%s") % test_program.branch_path());
        config_to_args(_config, args);
        args.push_back(_test_case.name());
        process::exec(test_program, args);
    }
};


/// Functor to execute a test case in a subprocess.
class execute_test_case_cleanup {
    engine::test_case _test_case;
    fs::path _work_directory;
    engine::properties_map _config;

public:
    /// Constructor for the functor.
    ///
    /// \param test_case_ The data of the test case, including the path to the
    ///     test program that contains it, the test case name and its metadata.
    /// \param work_directory_ The path to the directory to chdir into when
    ///     running the test program.
    /// \param config_ The configuration variables provided by the user.
    execute_test_case_cleanup(const engine::test_case& test_case_,
                              const fs::path& work_directory_,
                              const engine::properties_map& config_) :
        _test_case(test_case_),
        _work_directory(work_directory_),
        _config(config_)
    {
    }

    /// Entry point for the functor.
    void
    operator()(void) UTILS_NORETURN
    {
        const fs::path test_program = (
            _test_case.program().is_absolute() ? _test_case.program() :
            fs::current_path() / _test_case.program());

        isolate_process(_work_directory);

        std::vector< std::string > args;
        args.push_back(F("-s%s") % test_program.branch_path());
        config_to_args(_config, args);
        args.push_back(F("%s:cleanup") % _test_case.name());
        process::exec(test_program, args);
    }
};


/// Auxiliary function to execute a test case.
///
/// This is an auxiliary function for run_test_case that is protected from
/// leaking exceptions.  Any exception not managed here is probably a mistake,
/// but is correctly captured in the caller.
///
/// \param test_case The test case to execute.
///
/// \return The result of the execution of the test case.
static std::auto_ptr< const results::base_result >
run_test_case_safe(const engine::test_case& test_case,
                   const engine::properties_map& config)
{
    fs::auto_directory workdir(create_work_directory());

    const fs::path rundir(workdir.directory() / "run");
    fs::mkdir(rundir, 0755);

    const fs::path resfile(workdir.directory() / "result.txt");

    std::auto_ptr< process::child_with_files > body_child =
        process::child_with_files::fork(execute_test_case_body(
            test_case, resfile, rundir, config),
            workdir.directory() / "stdout.txt",
            workdir.directory() / "stderr.txt");
    const process::status body_status = body_child->wait();

    std::auto_ptr< process::child_with_files > cleanup_child =
        process::child_with_files::fork(execute_test_case_cleanup(
            test_case, rundir, config),
            workdir.directory() / "cleanup-stdout.txt",
            workdir.directory() / "cleanup-stderr.txt");
    const process::status cleanup_status = cleanup_child->wait();

    const bool timed_out = false;  // TODO(jmmv): Implement timeout detection.

    std::auto_ptr< const results::base_result > result = results::adjust(
        results::load(resfile), body_status, timed_out);
    workdir.cleanup();
    return result;
}


}  // anonymous namespace


/// Destructor for the hooks.
runner::hooks::~hooks(void)
{
}


/// Runs a single test case in a controlled manner.
///
/// All exceptions raised at run time are captured and reported as a test
/// failure.  These exceptions may be really bugs in our code, but we do not
/// want them to crash the runtime system.
///
/// \param test_case The test to execute.
/// \param config The configuration variables provided by the user.
///
/// \return The result of the test case execution.
std::auto_ptr< const results::base_result >
runner::run_test_case(const engine::test_case& test_case,
                      const engine::properties_map& config)
{
    std::auto_ptr< const results::base_result > result;
    try {
        result = run_test_case_safe(test_case, config);
    } catch (const std::exception& e) {
        result = results::make_result(results::broken(F(
            "The test caused an error in the runtime system: %s") % e.what()));
    }
    INV(result.get() != NULL);
    return result;
}


/// Runs a test program in a controlled manner.
///
/// If the test program fails to provide a list of test cases, a fake test case
/// named '__test_program__' is created and it is reported as broken.
///
/// \param test_program The test program to execute.
/// \param config The configuration variables provided by the user.
/// \param hooks Callbacks for events.
void
runner::run_test_program(const fs::path& test_program,
                         const engine::properties_map& config,
                         runner::hooks* hooks)
{
    engine::test_cases_vector test_cases;
    try {
        test_cases = engine::load_test_cases(test_program);
    } catch (const std::exception& e) {
        const results::broken broken(F("Failed to load list of test cases: "
                                       "%s") % e.what());
        hooks->start_test_case(test_program, "__test_program__");
        hooks->finish_test_case(test_program, "__test_program__",
                                results::make_result(broken));
        return;
    }

    for (engine::test_cases_vector::const_iterator iter = test_cases.begin();
         iter != test_cases.end(); iter++) {
        const engine::test_case& test_case = *iter;

        hooks->start_test_case(test_case.program(), test_case.name());
        std::auto_ptr< const results::base_result > result = run_test_case(
            test_case, config);
        hooks->finish_test_case(test_case.program(), test_case.name(),
                                result);
    }
}


/// Runs a collection of test programs (aka a test suite).
///
/// \param suite The definition of the test suite.
/// \param config The configuration variables provided by the user.
/// \param hooks Callbacks for events.
void
runner::run_test_suite(const engine::suite_config& suite,
                       const engine::properties_map& config,
                       runner::hooks* run_hooks)
{
    const std::vector< fs::path >& test_programs = suite.test_programs();
    for (std::vector< fs::path >::const_iterator iter = test_programs.begin();
         iter != test_programs.end(); iter++) {
        run_test_program(*iter, config, run_hooks);
    }
}
