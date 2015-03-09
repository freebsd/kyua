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

#include "engine/executor.hpp"

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

extern "C" {
#include <sys/types.h>
#include <sys/wait.h>

#include <signal.h>
}

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <stdexcept>

#include "engine/config.hpp"
#include "engine/exceptions.hpp"
#include "engine/isolation.hpp"
#include "engine/requirements.hpp"
#include "engine/runner.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "utils/config/tree.ipp"
#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/auto_cleaners.hpp"
#include "utils/fs/directory.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/logging/macros.hpp"
#include "utils/logging/operations.hpp"
#include "utils/noncopyable.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"
#include "utils/process/child.ipp"
#include "utils/process/operations.hpp"
#include "utils/process/status.hpp"
#include "utils/sanity.hpp"
#include "utils/shared_ptr.hpp"
#include "utils/signals/interrupts.hpp"
#include "utils/signals/timer.hpp"
#include "utils/stacktrace.hpp"
#include "utils/stream.hpp"

namespace config = utils::config;
namespace datetime = utils::datetime;
namespace executor = engine::executor;
namespace fs = utils::fs;
namespace logging = utils::logging;
namespace passwd = utils::passwd;
namespace process = utils::process;
namespace runner = engine::runner;
namespace signals = utils::signals;

using utils::none;
using utils::optional;


namespace {


/// Template for temporary directories created by the executor.
static const char* work_directory_template = PACKAGE_TARNAME ".XXXXXX";


/// Basename of the file containing the stdout of the test.
static const char* stdout_name = "stdout.txt";


/// Basename of the file containing the stderr of the test.
static const char* stderr_name = "stderr.txt";


/// Magic exit status to indicate that the test case was probably skipped.
///
/// The test case was only skipped if and only if we return this exit code and
/// we find the skipped_cookie file on disk.
static const int exit_skipped = 84;


/// Text file containing the skip reason for the test case.
///
/// This will only be present within unique_work_directory if the test case
/// exited with the exit_skipped code.  However, there is no guarantee that the
/// file is there (say if the test really decided to exit with code exit_skipped
/// on its own).
static const char* skipped_cookie = "skipped.txt";


/// Basename of the subdirectory in which the test is actually executed.
///
/// This is a subdirectory of the "unique work directory" generated for the test
/// case so that the control files created by us here are not clobbered by the
/// test's activity.
static const char* work_subdir = "work";


/// Mapping of interface names to interface definitions.
typedef std::map< std::string, std::shared_ptr< executor::interface > >
    interfaces_map;


/// Mapping of interface names to interface definitions.
///
/// Use register_interface() to add an entry to this global table.
static interfaces_map interfaces;


/// Scans the contents of a directory and appends the file listing to a file.
///
/// \param dir_path The directory to scan.
/// \param output_file The file to which to append the listing.
///
/// \throw engine::error If there are problems listing the files.
static void
append_files_listing(const fs::path& dir_path, const fs::path& output_file)
{
    std::ofstream output(output_file.c_str(), std::ios::app);
    if (!output)
        throw engine::error(F("Failed to open output file %s for append")
                            % output_file);
    try {
        output << "Files left in work directory after failure:\n";

        const fs::directory dir(dir_path);
        for (fs::directory::const_iterator iter = dir.begin();
             iter != dir.end(); ++iter) {
            if (iter->name != "." && iter->name != "..")
                output << iter->name << '\n';
        }
    } catch (const fs::error& e) {
        throw engine::error(F("Cannot append files listing to %s: %s")
                            % output_file % e.what());
    }
}


/// A timer that forcibly kills a subprocess on activation.
class deadline_killer : public signals::timer {
    /// PID of the process (and process group) to kill.
    const pid_t _pid;

    /// Timer activation callback.
    void
    callback(void)
    {
        ::killpg(_pid, SIGKILL);
        ::kill(_pid, SIGKILL);
    }

public:
    /// Constructor.
    ///
    /// \param delta Time to the timer activation.
    /// \param pid PID of the process (and process group) to kill.
    deadline_killer(const datetime::delta& delta, const pid_t pid) :
        signals::timer(delta), _pid(pid)
    {
    }
};


/// Maintenance data held while a test is being executed.
///
/// This data structure exists from the moment when a test is executed via
/// executor::spawn_test() to when it is cleaned up with
/// result_handle::cleanup().
struct exec_data {
    /// Path to the test case-specific work directory.
    fs::path unique_work_directory;

    /// Test program-specific execution interface.
    std::shared_ptr< executor::interface > interface;

    /// Test program data for this test case.
    model::test_program_ptr test_program;

    /// Name of the test case.
    std::string test_case_name;

    /// Start time.
    datetime::timestamp start_time;

    /// Timer to kill the test on activation.
    std::shared_ptr< deadline_killer > timer;

    /// Gets the timeout of a test case.
    ///
    /// TODO(jmmv): Due to how the metadata is represented, this happily ignores
    /// any test program-level metadata.  We should change the model so that
    /// test cases "inherit" their container metadata when they themselves do
    /// not explicitly override properties.  Easy to do when we load test cases
    /// externally... but not trivial internally with the current
    /// representation.  Not a big problem because in the real world we always
    /// load test programs externally, but the behavior is not obvious and has
    /// some odd consequences on the tests.
    ///
    /// \param test_program_ The test program containing the test case.
    /// \param test_case_name_ Name of the test case from which to get the
    ///     timeout.
    ///
    /// \return The timeout of the test case.
    static const datetime::delta
    get_timeout(const model::test_program_ptr test_program_,
                const std::string& test_case_name_)
    {
        const model::test_case& test_case =
            test_program_->find(test_case_name_);
        return test_case.get_metadata().timeout();
    }

    /// Constructor.
    ///
    /// \param unique_work_directory_ Path to the test case-specific work
    ///     directory.
    /// \param interface_ Test program-specific execution interface.
    /// \param test_program_ Test program data for this test case.
    /// \param test_case_name_ Name of the test case.
    /// \param start_time_ Timestamp of when this object was constructed.
    /// \param test_pid PID of the forked test case, for timeout enforcement
    ///     purposes.
    exec_data(const fs::path& unique_work_directory_,
              const std::shared_ptr< executor::interface >& interface_,
              const model::test_program_ptr test_program_,
              const std::string& test_case_name_,
              const datetime::timestamp& start_time_,
              const pid_t test_pid) :
        unique_work_directory(unique_work_directory_), interface(interface_),
        test_program(test_program_), test_case_name(test_case_name_),
        start_time(start_time_),
        timer(new deadline_killer(get_timeout(test_program_, test_case_name_),
                                  test_pid))
    {
    }
};


/// Mapping of active test case handles to their maintenance data.
typedef std::map< executor::exec_handle, exec_data > exec_data_map;


/// Functor to execute a test program in a child process.
class run_test_program {
    /// Interface of the test program to execute.
    std::shared_ptr< executor::interface > _interface;

    /// Test program to execute.
    const model::test_program_ptr _test_program;

    /// Name of the test case to execute.
    const std::string& _test_case_name;

    /// Path to the skip cookie to create, if needed.
    const fs::path& _skipped_cookie_path;

    /// Directory where the interface may place control files.
    const fs::path& _control_directory;

    /// Directory to enter when running the test program.
    ///
    /// This is a subdirectory of _control_directory but is separate so that
    /// test case operations do not inadvertently affect our files.
    const fs::path& _work_directory;

    /// User-provided configuration variables.
    const config::tree& _user_config;

    /// Calls with engine::isolate_child after guessing the unprivileged_user.
    void
    do_isolate_child(void)
    {
        const model::test_case& test_case = _test_program->find(
            _test_case_name);

        optional< passwd::user > unprivileged_user;
        if (_user_config.is_set("unprivileged_user") &&
            test_case.get_metadata().required_user() == "unprivileged") {
            unprivileged_user = _user_config.lookup< engine::user_node >(
                "unprivileged_user");
        }

        engine::isolate_path(unprivileged_user, _control_directory);
        engine::isolate_child(unprivileged_user, _work_directory);
    }

    /// Verifies if the test case needs to be skipped or not.
    ///
    /// We could very well run this on the executor parent process before
    /// issuing the fork.  However, doing this here in the child process is
    /// better for two reasons: first, it allows us to continue using the simple
    /// spawn/wait abstraction of the executor; and, second, we parallelize the
    /// requirements checks among tests.
    ///
    /// \post If the test's preconditions are not met, the caller process is
    /// terminated with a special exit code and a "skipped cookie" is written to
    /// the disk with the reason for the failure.
    void
    do_requirements_check(void)
    {
        const model::test_case& test_case = _test_program->find(
            _test_case_name);

        const std::string skip_reason = engine::check_reqs(
            test_case.get_metadata(), _user_config,
            _test_program->test_suite_name(),
            _work_directory);
        if (skip_reason.empty())
            return;

        std::ofstream output(_skipped_cookie_path.c_str());
        if (!output) {
            std::perror((F("Failed to open %s for write") %
                         _skipped_cookie_path).str().c_str());
            std::abort();
        }
        output << skip_reason;
        output.close();

        // Abruptly terminate the process.  We don't want to run any destructors
        // inherited from the parent process by mistake, which could, for
        // example, delete our own control files!
        ::_exit(exit_skipped);
    }

public:
    /// Constructor.
    /// \param interface Interface of the test program to execute.
    /// \param test_program Test program to execute.
    /// \param test_case_name Name of the test case to execute.
    /// \param skipped_cookie_path Path to the skip cookie to create, if needed.
    /// \param control_directory Directory where control files can be placed.
    /// \param work_directory Directory to enter when running the test program.
    /// \param user_config User-provided configuration variables.
    run_test_program(
        const std::shared_ptr< executor::interface > interface,
        const model::test_program_ptr test_program,
        const std::string& test_case_name,
        const fs::path& skipped_cookie_path,
        const fs::path& control_directory,
        const fs::path& work_directory,
        const config::tree& user_config) :
        _interface(interface),
        _test_program(test_program),
        _test_case_name(test_case_name),
        _skipped_cookie_path(skipped_cookie_path),
        _control_directory(control_directory),
        _work_directory(work_directory),
        _user_config(user_config)
    {
    }

    /// Body of the subprocess.
    void
    operator()(void)
    {
        logging::set_inmemory();

        const model::test_case& test_case = _test_program->find(
            _test_case_name);
        if (test_case.fake_result())
            ::_exit(EXIT_SUCCESS);

        do_isolate_child();
        do_requirements_check();

        const config::properties_map vars = runner::generate_tester_config(
            _user_config, _test_program->test_suite_name());
        _interface->exec_test(*_test_program, _test_case_name, vars,
                              _control_directory);
    }
};


/// Obtains the right executor interface for a given test program.
///
/// \param name The name of the interface of the test program.
///
/// \return An executor interface.
std::shared_ptr< executor::interface >
find_interface(const std::string& name)
{
    const interfaces_map::const_iterator iter = interfaces.find(name);
    PRE(interfaces.find(name) != interfaces.end());
    return (*iter).second;
}


}  // anonymous namespace


/// Internal implementation for the result_handle class.
struct engine::executor::result_handle::impl {
    /// Original exec_handle corresponding to this result.
    const executor::exec_handle exec_handle;

    /// Test program data for this test case.
    model::test_program_ptr test_program;

    /// Name of the test case.
    std::string test_case_name;

    /// The actual result of the test execution.
    const model::test_result test_result;

    /// Timestamp of when spawn_test was called.
    const datetime::timestamp start_time;

    /// Timestamp of when wait_any_test returned this object.
    const datetime::timestamp end_time;

    /// Path to the test-specific work directory.
    const fs::path unique_work_directory;

    /// Path to the test's stdout file.
    const fs::path stdout_file;

    /// Path to the test's stderr file.
    const fs::path stderr_file;

    /// Mutable pointer to the corresponding executor state.
    ///
    /// This object references a member of the executor_handle that yielded this
    /// result_handle instance.  We need this direct access to clean up after
    /// ourselves when the result is destroyed.
    exec_data_map& all_exec_data;

    /// Whether the test state has been cleaned yet or not.
    ///
    /// Used to keep track of explicit calls to the public cleanup().
    bool cleaned;

    /// Constructor.
    ///
    /// \param exec_handle_ Original exec_handle corresponding to this result.
    /// \param test_program_ Test program data for this test case.
    /// \param test_case_name_ Name of the test case.
    /// \param test_result_ The actual result of the test execution.
    /// \param start_time_ Timestamp of when spawn_test was called.
    /// \param end_time_ Timestamp of when wait_any_test returned this object.
    /// \param unique_work_directory_ Path to the test-specific work directory.
    /// \param stdout_file_ Path to the test's stdout file.
    /// \param stderr_file_ Path to the test's stderr file.
    /// \param [in,out] all_exec_data_ Global object keeping track of all active
    ///     executions for an executor.  This is a pointer to a member of the
    ///     executor_handle object.
    impl(const executor::exec_handle exec_handle_,
         const model::test_program_ptr test_program_,
         const std::string& test_case_name_,
         const model::test_result& test_result_,
         const datetime::timestamp& start_time_,
         const datetime::timestamp& end_time_,
         const fs::path& unique_work_directory_,
         const fs::path& stdout_file_,
         const fs::path& stderr_file_,
         exec_data_map& all_exec_data_) :
        exec_handle(exec_handle_), test_program(test_program_),
        test_case_name(test_case_name_), test_result(test_result_),
        start_time(start_time_), end_time(end_time_),
        unique_work_directory(unique_work_directory_),
        stdout_file(stdout_file_), stderr_file(stderr_file_),
        all_exec_data(all_exec_data_), cleaned(false)
    {
    }

    /// Destructor.
    ~impl(void)
    {
        if (!cleaned) {
            LW("Implicitly cleaning up test case; ignoring errors!");
            try {
                cleanup();
            } catch (const std::runtime_error& error) {
                LE(F("Test case cleanup failed: %s") % error.what());
            }
        }
    }

    /// Cleans up the test case results.
    ///
    /// \throw engine::error If the cleanup fails, especially due to the
    ///     inability to remove the work directory.
    void
    cleanup(void)
    {
        LI(F("Cleaning up result_handle for exec_handle %s") % exec_handle);

        fs::rm_r(unique_work_directory);
        all_exec_data.erase(exec_handle);

        cleaned = true;
    }
};


/// Constructor.
///
/// \param pimpl Constructed internal implementation.
executor::result_handle::result_handle(std::shared_ptr< impl > pimpl) :
    _pimpl(pimpl)
{
}


/// Destructor.
executor::result_handle::~result_handle(void)
{
    if (!_pimpl->cleaned) {
        LW(F("Implicitly cleaning up result handle for exec handle %s; "
               "ignoring errors!") % _pimpl->exec_handle);
        try {
            _pimpl->cleanup();
        } catch (const std::runtime_error& error) {
            LE(F("Test case cleanup failed: %s") % error.what());
        }
    }
}


/// Cleans up the test case results.
///
/// This function should be called explicitly as it provides the means to
/// control any exceptions raised during cleanup.  Do not rely on the destructor
/// to clean things up.
///
/// \throw engine::error If the cleanup fails, especially due to the inability
///     to remove the work directory.
void
executor::result_handle::cleanup(void)
{
    PRE(!_pimpl->cleaned);
    _pimpl->cleanup();
    POST(_pimpl->cleaned);
}


/// Returns the original exec_handle corresponding to this result.
///
/// \return An exec_handle.
executor::exec_handle
executor::result_handle::original_exec_handle(void) const
{
    return _pimpl->exec_handle;
}


/// Returns the test program that yielded this result.
///
/// \return A test program.
const model::test_program_ptr
executor::result_handle::test_program(void) const
{
    return _pimpl->test_program;
}


/// Returns the name of the test case that yielded this result.
///
/// \return A test case name
const std::string&
executor::result_handle::test_case_name(void) const
{
    return _pimpl->test_case_name;
}


/// Returns the actual result of the test execution.
///
/// \return A test result.
const model::test_result&
executor::result_handle::test_result(void) const
{
    return _pimpl->test_result;
}


/// Returns the timestamp of when spawn_test was called.
///
/// \return A timestamp.
const datetime::timestamp&
executor::result_handle::start_time(void) const
{
    return _pimpl->start_time;
}


/// Returns the timestamp of when wait_any_test returned this object.
///
/// \return A timestamp.
const datetime::timestamp&
executor::result_handle::end_time(void) const
{
    return _pimpl->end_time;
}


/// Returns the path to the test-specific work directory.
///
/// This is guaranteed to be clear of files created by the executor.
///
/// \return The path to a directory that exists until cleanup() is called.
fs::path
executor::result_handle::work_directory(void) const
{
    return _pimpl->unique_work_directory / work_subdir;
}


/// Returns the path to the test's stdout file.
///
/// \return The path to a file that exists until cleanup() is called.
const fs::path&
executor::result_handle::stdout_file(void) const
{
    return _pimpl->stdout_file;
}


/// Returns the path to the test's stderr file.
///
/// \return The path to a file that exists until cleanup() is called.
const fs::path&
executor::result_handle::stderr_file(void) const
{
    return _pimpl->stderr_file;
}


/// Internal implementation for the executor_handle.
///
/// Note that this object is a very thin wrapper of the "state "of the
/// executor.  This is on purpose: while we could keep here all the global
/// variables required by the executor, the code would become messier; it's
/// easier to just use global variables considering that the executor is a
/// singleton.
struct engine::executor::executor_handle::impl {
    /// Numeric counter of executed tests, for identification purposes.
    size_t last_test;

    /// Interrupts handler.
    std::auto_ptr< signals::interrupts_handler > interrupts_handler;

    /// Root work directory for all executed test cases.
    std::auto_ptr< fs::auto_directory > root_work_directory;

    /// Mapping of exec handles to the data required at run time.
    exec_data_map all_exec_data;

    /// Whether the executor state has been cleaned yet or not.
    ///
    /// Used to keep track of explicit calls to the public cleanup().
    bool cleaned;

    /// Constructor.
    impl(void) :
        last_test(0),
        interrupts_handler(new signals::interrupts_handler()),
        root_work_directory(new fs::auto_directory(
            fs::auto_directory::mkdtemp(work_directory_template))),
        cleaned(false)
    {
    }

    /// Destructor.
    ~impl(void)
    {
        if (!cleaned) {
            LW("Implicitly cleaning up executor; ignoring errors!");
            try {
                cleanup();
                cleaned = true;
            } catch (const std::runtime_error& error) {
                LE(F("Executor global cleanup failed: %s") % error.what());
            }
        }
    }

    /// Cleans up the executor state.
    void
    cleanup(void)
    {
        PRE(!cleaned);

        for (exec_data_map::const_iterator iter = all_exec_data.begin();
             iter != all_exec_data.end(); ++iter) {
            const exec_handle& pid = (*iter).first;
            const exec_data& data = (*iter).second;

            LW(F("Killing subprocess (and group) %s") % pid);
            // Yes, killing both the process and the process groups is the
            // correct thing to do here.  We need to deal with the case where
            // the subprocess has been created but has not yet had a chance to
            // execute setpgrp(2) or setsid(2), in which case there is no
            // process group with this identifier yet.
            //
            // One would think that checking for killpg(2)'s error code and
            // running kill(2) only when the former has failed would be nicer,
            // but that's not the case because this would be racy.  Consider the
            // scenario where we fail to invoke killpg(2), the subprocess
            // finishes its setup and spawns other subsubprocesses, and then we
            // execute kill(2): we would miss out some processes.  Killing the
            // top-level process explicitly first ensures that it cannot make
            // forward progress in any case.
            (void)::kill(pid, SIGKILL);
            (void)::killpg(pid, SIGKILL);
            int status;
            if (::waitpid(pid, &status, 0) == -1) {
                // Should not happen.
                LW(F("Failed to wait for PID %s") % pid);
            }

            try {
                fs::rm_r(data.unique_work_directory);
            } catch (const fs::error& e) {
                LE(F("Failed to clean up test case work directory %s: %s") %
                   data.unique_work_directory % e.what());
            }
        }
        all_exec_data.clear();

        try {
            // The following only causes the work directory to be deleted, not
            // any of its contents, so we expect this to always succeed.  This
            // *should* be sufficient because, in the loop above, we have
            // individually wiped the subdirectories of any still-unclean test
            // cases.
            root_work_directory->cleanup();
        } catch (const fs::error& e) {
            LE(F("Failed to clean up executor work directory %s: %s; this is "
                 "an internal error") % root_work_directory->directory()
               % e.what());
        }
        root_work_directory.reset(NULL);

        interrupts_handler->unprogram();
        interrupts_handler.reset(NULL);
    }
};


/// Constructor.
executor::executor_handle::executor_handle(void) throw() : _pimpl(new impl())
{
}


/// Destructor.
executor::executor_handle::~executor_handle(void)
{
}


/// Queries the path to the root of the work directory for all tests.
///
/// \return A path.
const fs::path&
executor::executor_handle::root_work_directory(void) const
{
    return _pimpl->root_work_directory->directory();
}


/// Cleans up the executor state.
///
/// This function should be called explicitly as it provides the means to
/// control any exceptions raised during cleanup.  Do not rely on the destructor
/// to clean things up.
///
/// \throw engine::error If there are problems cleaning up the executor.
void
executor::executor_handle::cleanup(void)
{
    PRE(!_pimpl->cleaned);
    _pimpl->cleanup();
    _pimpl->cleaned = true;
}


/// Registers a new interface.
///
/// \param name The name of the interface.  Must not have yet been registered.
/// \param spec Interface specification.
void
executor::register_interface(const std::string& name,
                             const std::shared_ptr< interface > spec)
{
    PRE(interfaces.find(name) == interfaces.end());
    interfaces.insert(interfaces_map::value_type(name, spec));
}


/// Initializes the executor.
///
/// \pre This function can only be called if there is no other executor_handle
/// object alive.
///
/// \return A handle to the operations of the executor.
executor::executor_handle
executor::setup(void)
{
    return executor_handle();
}


/// Forks and executes a test case asynchronously.
///
/// \param test_program The container test program.
/// \param test_case_name The name of the test case to run.
/// \param user_config User-provided configuration variables.
///
/// \return A handle for the background operation.  Used to match the result of
/// the execution returned by wait_any_test() with this invocation.
executor::exec_handle
executor::executor_handle::spawn_test(
    const model::test_program_ptr test_program,
    const std::string& test_case_name,
    const config::tree& user_config)
{
    signals::check_interrupt();

    ++_pimpl->last_test;

    const fs::path unique_work_directory =
        _pimpl->root_work_directory->directory() /
        (F("%s") % _pimpl->last_test);
    fs::mkdir_p(unique_work_directory / work_subdir, 0755);

    const std::shared_ptr< executor::interface > interface = find_interface(
        test_program->interface_name());

    LI(F("Spawning %s:%s") % test_program->absolute_path() % test_case_name);

    std::auto_ptr< process::child > child = process::child::fork_files(
        run_test_program(interface, test_program, test_case_name,
                         unique_work_directory / skipped_cookie,
                         unique_work_directory,
                         unique_work_directory / work_subdir,
                         user_config),
        unique_work_directory / stdout_name,
        unique_work_directory / stderr_name);
    const executor::exec_handle handle = child->pid();

    const exec_data data(unique_work_directory, interface, test_program,
                         test_case_name, datetime::timestamp::now(),
                         child->pid());
    _pimpl->all_exec_data.insert(exec_data_map::value_type(
        child->pid(), data));
    LI(F("Spawned test with exec_handle %s") % handle);
    return handle;
}


/// Waits for completion of any forked test case.
///
/// \return A (handle, test result) pair.
executor::result_handle
executor::executor_handle::wait_any_test(void)
{
    signals::check_interrupt();

    const process::status status = process::wait_any();
    const executor::exec_handle handle = status.dead_pid();
    LI(F("Waited for test with exec_handle %s") % handle);

    (void)::killpg(status.dead_pid(), SIGKILL);

    const exec_data_map::iterator iter = _pimpl->all_exec_data.find(handle);
    exec_data& data = (*iter).second;
    data.timer->unprogram();

    INV(!data.timer->fired() ||
        (status.signaled() && status.termsig() == SIGKILL));

    const fs::path stdout_path = data.unique_work_directory / stdout_name;
    const fs::path stderr_path = data.unique_work_directory / stderr_name;

    utils::dump_stacktrace_if_available(
        data.test_program->absolute_path(),
        utils::make_optional(status),
        data.unique_work_directory / work_subdir,
        stderr_path);

    const model::test_case& test_case = data.test_program->find(
        data.test_case_name);

    optional< model::test_result > result = test_case.fake_result();
    if (!result && status.exited() && status.exitstatus() == exit_skipped) {
        // If the test's process terminated with our magic "exit_skipped"
        // status, there are two cases to handle.  The first is the case where
        // the "skipped cookie" exists, in which case we never got to actually
        // invoke the test program; if that's the case, handle it here.  The
        // second case is where the test case actually decided to exit with the
        // "exit_skipped" status; in that case, just fall back to the regular
        // status handling.
        const fs::path skipped_cookie_path = data.unique_work_directory /
            skipped_cookie;
        std::ifstream input(skipped_cookie_path.c_str());
        if (input) {
            result = model::test_result(model::test_result_skipped,
                                        utils::read_stream(input));
            input.close();
        }
    }
    if (!result) {
        result = data.interface->compute_result(
            data.timer->fired() ? none : utils::make_optional(status),
            data.unique_work_directory, stdout_path, stderr_path);
    }
    INV(result);

    if (!result.get().good()) {
        append_files_listing(data.unique_work_directory / work_subdir,
                             stderr_path);
    }

    std::shared_ptr< result_handle::impl > result_handle_impl(
        new result_handle::impl(
            handle,
            data.test_program, data.test_case_name, result.get(),
            data.start_time, datetime::timestamp::now(),
            data.unique_work_directory,
            stdout_path, stderr_path,
            _pimpl->all_exec_data));
    return result_handle(result_handle_impl);
}


/// Checks if an interrupt has fired.
///
/// Calls to this function should be sprinkled in strategic places through the
/// code protected by an interrupts_handler object.
///
/// This is just a wrapper over signals::check_interrupt() to avoid leaking this
/// dependency to the caller.
///
/// \throw signals::interrupted_error If there has been an interrupt.
void
executor::executor_handle::check_interrupt(void) const
{
    signals::check_interrupt();
}
