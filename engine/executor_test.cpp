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

extern "C" {
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <signal.h>
#include <unistd.h>
}

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

#include <atf-c++.hpp>

#include "engine/config.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "utils/config/tree.ipp"
#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/env.hpp"
#include "utils/format/containers.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"
#include "utils/process/status.hpp"
#include "utils/sanity.hpp"
#include "utils/signals/exceptions.hpp"
#include "utils/stacktrace.hpp"
#include "utils/text/exceptions.hpp"
#include "utils/text/operations.ipp"

namespace config = utils::config;
namespace datetime = utils::datetime;
namespace executor = engine::executor;
namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace process = utils::process;
namespace signals = utils::signals;
namespace text = utils::text;

using utils::none;
using utils::optional;


namespace {


/// Checks if a string starts with a prefix.
///
/// \param str The string to be tested.
/// \param prefix The prefix to look for.
///
/// \return True if the string is prefixed as specified.
static bool
starts_with(const std::string& str, const std::string& prefix)
{
    return (str.length() >= prefix.length() &&
            str.substr(0, prefix.length()) == prefix);
}


/// Strips a prefix from a string and converts the rest to an integer.
///
/// \param str The string to be tested.
/// \param prefix The prefix to strip from the string.
///
/// \return The part of the string after the prefix converted to an integer.
static int
suffix_to_int(const std::string& str, const std::string& prefix)
{
    PRE(starts_with(str, prefix));
    try {
        return text::to_type< int >(str.substr(prefix.length()));
    } catch (const text::value_error& error) {
        std::cerr << F("Failed: %s\n") % error.what();
        std::abort();
    }
}


/// Mock interface definition for testing.
///
/// This executor interface does not execute external binaries.  It is designed
/// to simulate the executor of various programs with different exit statuses.
class mock_interface : public executor::interface {
    /// Executes the subprocess simulating an exec.
    ///
    /// This is just a simple wrapper over _exit(2) because we cannot use
    /// std::exit on exit from this mock interface.  The reason is that we do
    /// not want to invoke any destructors as otherwise we'd clear up the global
    /// executor state by mistake.  This wouldn't be a major problem if it
    /// wasn't because doing so deletes on-disk files and we want to leave them
    /// in place so that the parent process can test for them!
    ///
    /// \param exit_code Exit code.
    void
    do_exit(const int exit_code) const UTILS_NORETURN
    {
        std::cout.flush();
        std::cerr.flush();
        ::_exit(exit_code);
    }

    /// Executes a test case that creates a file in its work directory.
    ///
    /// \param id Number to suffix to the cookie file.
    void
    exec_cookie(const int id) const UTILS_NORETURN
    {
        atf::utils::create_file(F("cookie.%s") % id, "");
        do_exit(EXIT_SUCCESS);
    }

    /// Executes a test case that creates various files and then fails.
    void
    exec_create_files_and_fail(void) const UTILS_NORETURN
    {
        atf::utils::create_file("first file", "");
        atf::utils::create_file("second-file", "");
        fs::mkdir_p(fs::path("dir1/dir2"), 0755);
        ::kill(::getpid(), SIGTERM);
        std::abort();
    }

    /// Executes a test case that deletes all files in the current directory.
    ///
    /// This is intended to validate that the test runs in an empty directory,
    /// separate from any control files that the executor may have created.
    void
    exec_delete_all(void) const UTILS_NORETURN
    {
        const int exit_code = ::system("rm *") == -1
            ? EXIT_FAILURE : EXIT_SUCCESS;

        // Recreate our own cookie.
        atf::utils::create_file("exec_test_was_called", "");

        do_exit(exit_code);
    }

    /// Executes a test case that dumps user configuration.
    void
    exec_dump_unprivileged_user(void) const UTILS_NORETURN
    {
        const passwd::user current_user = passwd::current_user();
        std::cout << F("UID = %s\n") % current_user.uid;
        do_exit(EXIT_SUCCESS);
    }

    /// Executes a test case that returns a specific exit code.
    ///
    /// \param exit_code Exit status to terminate the program with.
    void
    exec_exit(const int exit_code) const UTILS_NORETURN
    {
        do_exit(exit_code);
    }

    /// Executes a test case that just blocks.
    void
    exec_pause(void) const UTILS_NORETURN
    {
        sigset_t mask;
        sigemptyset(&mask);
        for (;;) {
            ::sigsuspend(&mask);
        }
        std::abort();
    }

    /// Executes a test case that prints all input parameters to the functor.
    ///
    /// \param test_program The test program to execute.
    /// \param test_case_name Name of the test case to invoke, which must be a
    ///     number.
    /// \param vars User-provided variables to pass to the test program.
    void
    exec_print_params(const model::test_program& test_program,
                      const std::string& test_case_name,
                      const std::map< std::string, std::string >& vars) const
        UTILS_NORETURN
    {
        std::cout << F("Test program: %s\n") % test_program.relative_path();
        std::cout << F("Test case: %s\n") % test_case_name;
        for (std::map< std::string, std::string >::const_iterator iter =
                 vars.begin(); iter != vars.end(); ++iter) {
            std::cout << F("%s=%s\n") % (*iter).first % (*iter).second;
        }

        std::cerr << F("stderr: %s\n") % test_case_name;

        do_exit(EXIT_SUCCESS);
    }

    /// Executes a test that sleeps for a period of time before exiting.
    ///
    /// \param seconds Number of seconds to sleep for.
    void
    exec_sleep(const int seconds) const UTILS_NORETURN
    {
        ::sleep(seconds);
        do_exit(EXIT_SUCCESS);
    }

    /// Executes a test that spawns a subchild that gets stuck.
    ///
    /// This test case is used by the caller to validate that the whole process
    /// tree is terminated when the test case is killed.
    void
    exec_spawn_blocking_child(void) const UTILS_NORETURN
    {
        pid_t pid = ::fork();
        if (pid == -1) {
            std::cerr << "Cannot fork subprocess\n";
            do_exit(EXIT_FAILURE);
        } else if (pid == 0) {
            for (;;)
                ::pause();
        } else {
            const fs::path name = fs::path(utils::getenv("CONTROL_DIR").get()) /
                "pid";
            std::ofstream pidfile(name.c_str());
            if (!pidfile) {
                std::cerr << "Failed to create the pidfile\n";
                do_exit(EXIT_FAILURE);
            }
            pidfile << pid;
            pidfile.close();
            do_exit(EXIT_SUCCESS);
        }
    }

    /// Executes a test that checks if isolate_child() has been called.
    void
    exec_validate_isolation(void) const UTILS_NORETURN
    {
        if (utils::getenv("HOME").get() == "fake-value") {
            std::cerr << "HOME not reset\n";
            do_exit(EXIT_FAILURE);
        }
        if (utils::getenv("LANG")) {
            std::cerr << "LANG not unset\n";
            do_exit(EXIT_FAILURE);
        }
        do_exit(EXIT_SUCCESS);
    }

public:
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
    exec_test(const model::test_program& test_program,
              const std::string& test_case_name,
              const std::map< std::string, std::string >& vars,
              const fs::path& control_directory) const
    {
        const fs::path cookie = control_directory / "exec_test_was_called";
        std::ofstream control_file(cookie.c_str());
        if (!control_file) {
            std::cerr << "Failed to create " << cookie << '\n';
            std::abort();
        }

        if (starts_with(test_case_name, "cookie ")) {
            exec_cookie(suffix_to_int(test_case_name, "cookie "));
        } else if (starts_with(test_case_name, "create_files_and_fail")) {
            exec_create_files_and_fail();
        } else if (test_case_name == "delete_all") {
            exec_delete_all();
        } else if (test_case_name == "dump_unprivileged_user") {
            exec_dump_unprivileged_user();
        } else if (starts_with(test_case_name, "exit ")) {
            exec_exit(suffix_to_int(test_case_name, "exit "));
        } else if (test_case_name == "pause") {
            exec_pause();
        } else if (starts_with(test_case_name, "print_params")) {
            exec_print_params(test_program, test_case_name, vars);
        } else if (starts_with(test_case_name, "sleep ")) {
            exec_sleep(suffix_to_int(test_case_name, "sleep "));
        } else if (test_case_name == "spawn_blocking_child") {
            exec_spawn_blocking_child();
        } else if (test_case_name == "validate_isolation") {
            exec_validate_isolation();
        } else {
            std::cerr << "Unknown test case " << test_case_name << '\n';
            std::abort();
        }
    }

    /// Computes the result of a test case based on its termination status.
    ///
    /// \param status The termination status of the subprocess used to execute
    ///     the exec_test() method or none if the test timed out.
    /// \param control_directory Path to the directory where the interface may
    ///     have placed control files.
    /// \param stdout_path Path to the file containing the stdout of the test.
    /// \param stderr_path Path to the file containing the stderr of the test.
    ///
    /// \return A test result.
    model::test_result
    compute_result(const optional< process::status >& status,
                   const fs::path& control_directory,
                   const fs::path& stdout_path,
                   const fs::path& stderr_path) const
    {
        // Do not use any ATF_* macros here.  Some of the tests below invoke
        // this code in a subprocess, and terminating such subprocess due to a
        // failed ATF_* macro yields mysterious failures that are incredibly
        // hard to debug.  (Case in point: the signal_handling test is racy by
        // nature, and the test run by exec_test() above may not have created
        // the cookie we expect below.  We don't want to "silently" exit if the
        // file is not there.)

        if (!status) {
            return model::test_result(model::test_result_broken,
                                      "Timed out");
        }

        if (status.get().exited()) {
            // Only sanity-check the work directory-related parameters in case
            // of a clean exit.  In all other cases, there is no guarantee that
            // these were ever created.
            if (!atf::utils::file_exists(
                    (control_directory / "exec_test_was_called").str())) {
                return model::test_result(
                    model::test_result_broken,
                    "compute_result's control_directory does not seem to point "
                    "to the right location");
            }
            if (!atf::utils::file_exists(stdout_path.str())) {
                return model::test_result(
                    model::test_result_broken,
                    "compute_result's stdout_path does not exist");
            }
            if (!atf::utils::file_exists(stderr_path.str())) {
                return model::test_result(
                    model::test_result_broken,
                    "compute_result's stderr_path does not exist");
            }

            return model::test_result(
                model::test_result_passed,
                F("Exit %s") % status.get().exitstatus());
        } else {
            return model::test_result(
                model::test_result_failed,
                F("Signal %s") % status.get().termsig());
        }
    }
};


/// Ensures that a killed process is gone.
///
/// The way we do this is by sending an idempotent signal to the given PID
/// and checking if the signal was delivered.  If it was, the process is
/// still alive; if it was not, then it is gone.
///
/// Note that this might be inaccurate for two reasons:
///
/// 1) The system may have spawned a new process with the same pid as
///    our subchild... but in practice, this does not happen because
///    most systems do not immediately reuse pid numbers.  If that
///    happens... well, we get a false test failure.
///
/// 2) We ran so fast that even if the process was sent a signal to
///    die, it has not had enough time to process it yet.  This is why
///    we retry this a few times.
///
/// \param pid PID of the process to check.
static void
ensure_dead(const pid_t pid)
{
    int attempts = 30;
retry:
    if (::kill(pid, SIGCONT) != -1 || errno != ESRCH) {
        if (attempts > 0) {
            std::cout << "Subprocess not dead yet; retrying wait\n";
            --attempts;
            ::usleep(100000);
            goto retry;
        }
        ATF_FAIL(F("The subprocess %s of our child was not killed") % pid);
    }
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(integration__run_one);
ATF_TEST_CASE_BODY(integration__run_one)
{
    const model::test_program_ptr program = model::test_program_builder(
        "mock", fs::path("the-program"), fs::current_path(), "the-suite")
        .add_test_case("exit 41").build_ptr();

    const config::tree user_config = engine::empty_config();

    executor::executor_handle handle = executor::setup();

    const executor::exec_handle exec_handle = handle.spawn_test(
        program, "exit 41", user_config);

    executor::result_handle result_handle = handle.wait_any_test();
    ATF_REQUIRE_EQ(exec_handle, result_handle.original_exec_handle());
    ATF_REQUIRE_EQ(model::test_result(model::test_result_passed, "Exit 41"),
                   result_handle.test_result());
    result_handle.cleanup();

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__run_many);
ATF_TEST_CASE_BODY(integration__run_many)
{
    static const std::size_t num_test_programs = 30;

    const config::tree user_config = engine::empty_config();

    executor::executor_handle handle = executor::setup();

    // We mess around with the "current time" below, so make sure the tests do
    // not spuriously exceed their deadline by bumping it to a large number.
    const model::metadata infinite_timeout = model::metadata_builder()
        .set_timeout(datetime::delta(1000000L, 0)).build();

    std::size_t total_tests = 0;
    std::map< executor::exec_handle, model::test_program_ptr >
        exp_test_programs;
    std::map< executor::exec_handle, std::string > exp_test_case_names;
    std::map< executor::exec_handle, datetime::timestamp > exp_start_times;
    std::map< executor::exec_handle, int > exp_exit_statuses;
    for (std::size_t i = 0; i < num_test_programs; ++i) {
        const std::string test_case_0 = F("exit %s") % (i * 3 + 0);
        const std::string test_case_1 = F("exit %s") % (i * 3 + 1);
        const std::string test_case_2 = F("exit %s") % (i * 3 + 2);

        const model::test_program_ptr program = model::test_program_builder(
            "mock", fs::path(F("program-%s") % i),
            fs::current_path(), "the-suite")
            .add_test_case(test_case_0, infinite_timeout)
            .add_test_case(test_case_1, infinite_timeout)
            .add_test_case(test_case_2, infinite_timeout)
            .build_ptr();

        const datetime::timestamp start_time = datetime::timestamp::from_values(
            2014, 12, 8, 9, 40, 0, i);

        executor::exec_handle exec_handle;

        datetime::set_mock_now(start_time);
        exec_handle = handle.spawn_test(program, test_case_0, user_config);
        exp_test_programs.insert(std::make_pair(exec_handle, program));
        exp_test_case_names.insert(std::make_pair(exec_handle, test_case_0));
        exp_start_times.insert(std::make_pair(exec_handle, start_time));
        exp_exit_statuses.insert(std::make_pair(exec_handle, i * 3));
        ++total_tests;

        datetime::set_mock_now(start_time);
        exec_handle = handle.spawn_test(program, test_case_1, user_config);
        exp_test_programs.insert(std::make_pair(exec_handle, program));
        exp_test_case_names.insert(std::make_pair(exec_handle, test_case_1));
        exp_start_times.insert(std::make_pair(exec_handle, start_time));
        exp_exit_statuses.insert(std::make_pair(exec_handle, i * 3 + 1));
        ++total_tests;

        datetime::set_mock_now(start_time);
        exec_handle = handle.spawn_test(program, test_case_2, user_config);
        exp_test_programs.insert(std::make_pair(exec_handle, program));
        exp_test_case_names.insert(std::make_pair(exec_handle, test_case_2));
        exp_start_times.insert(std::make_pair(exec_handle, start_time));
        exp_exit_statuses.insert(std::make_pair(exec_handle, i * 3 + 2));
        ++total_tests;
    }

    for (std::size_t i = 0; i < total_tests; ++i) {
        const datetime::timestamp end_time = datetime::timestamp::from_values(
            2014, 12, 8, 9, 50, 10, i);
        datetime::set_mock_now(end_time);
        executor::result_handle result_handle = handle.wait_any_test();
        const executor::exec_handle exec_handle =
            result_handle.original_exec_handle();

        const model::test_program_ptr test_program = exp_test_programs.find(
            exec_handle)->second;
        const std::string& test_case_name = exp_test_case_names.find(
            exec_handle)->second;
        const datetime::timestamp& start_time = exp_start_times.find(
            exec_handle)->second;
        const int exit_status = exp_exit_statuses.find(exec_handle)->second;

        ATF_REQUIRE_EQ(model::test_result(model::test_result_passed,
                                          F("Exit %s") % exit_status),
                       result_handle.test_result());

        ATF_REQUIRE_EQ(test_program, result_handle.test_program());
        ATF_REQUIRE_EQ(test_case_name, result_handle.test_case_name());

        ATF_REQUIRE_EQ(start_time, result_handle.start_time());
        ATF_REQUIRE_EQ(end_time, result_handle.end_time());

        result_handle.cleanup();

        ATF_REQUIRE(!atf::utils::file_exists(
                        result_handle.stdout_file().str()));
        ATF_REQUIRE(!atf::utils::file_exists(
                        result_handle.stderr_file().str()));
        ATF_REQUIRE(!atf::utils::file_exists(
                        result_handle.work_directory().str()));
    }

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__parameters_and_output);
ATF_TEST_CASE_BODY(integration__parameters_and_output)
{
    const model::test_program_ptr program = model::test_program_builder(
        "mock", fs::path("the-program"), fs::current_path(), "the-suite")
        .add_test_case("print_params").build_ptr();

    config::tree user_config = engine::empty_config();
    user_config.set_string("test_suites.the-suite.one", "first variable");
    user_config.set_string("test_suites.the-suite.two", "second variable");

    executor::executor_handle handle = executor::setup();

    const executor::exec_handle exec_handle = handle.spawn_test(
        program, "print_params", user_config);

    executor::result_handle result_handle = handle.wait_any_test();

    ATF_REQUIRE_EQ(exec_handle, result_handle.original_exec_handle());
    ATF_REQUIRE_EQ(program, result_handle.test_program());
    ATF_REQUIRE_EQ("print_params", result_handle.test_case_name());
    ATF_REQUIRE_EQ(model::test_result(model::test_result_passed, "Exit 0"),
                   result_handle.test_result());

    ATF_REQUIRE(atf::utils::compare_file(
        result_handle.stdout_file().str(),
        "Test program: the-program\n"
        "Test case: print_params\n"
        "one=first variable\n"
        "two=second variable\n"));
    ATF_REQUIRE(atf::utils::compare_file(
        result_handle.stderr_file().str(), "stderr: print_params\n"));

    result_handle.cleanup();

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__timestamps);
ATF_TEST_CASE_BODY(integration__timestamps)
{
    const model::test_program_ptr program = model::test_program_builder(
        "mock", fs::path("the-program"), fs::current_path(), "the-suite")
        .add_test_case("exit 70").build_ptr();

    const config::tree user_config = engine::empty_config();

    executor::executor_handle handle = executor::setup();

    const datetime::timestamp start_time = datetime::timestamp::from_values(
        2014, 12, 8, 9, 35, 10, 1000);
    const datetime::timestamp end_time = datetime::timestamp::from_values(
        2014, 12, 8, 9, 35, 20, 2000);

    datetime::set_mock_now(start_time);
    (void)handle.spawn_test(program, "exit 70", user_config);

    datetime::set_mock_now(end_time);
    executor::result_handle result_handle = handle.wait_any_test();
    ATF_REQUIRE_EQ(model::test_result(model::test_result_passed, "Exit 70"),
                   result_handle.test_result());
    ATF_REQUIRE_EQ(start_time, result_handle.start_time());
    ATF_REQUIRE_EQ(end_time, result_handle.end_time());
    result_handle.cleanup();

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__files);
ATF_TEST_CASE_BODY(integration__files)
{
    const model::test_program_ptr program = model::test_program_builder(
        "mock", fs::path("the-program"), fs::current_path(), "the-suite")
        .add_test_case("cookie 12345").build_ptr();

    const config::tree user_config = engine::empty_config();

    executor::executor_handle handle = executor::setup();

    (void)handle.spawn_test(program, "cookie 12345", user_config);

    executor::result_handle result_handle = handle.wait_any_test();

    ATF_REQUIRE(atf::utils::file_exists(
                    (result_handle.work_directory() / "cookie.12345").str()));

    result_handle.cleanup();

    ATF_REQUIRE(!atf::utils::file_exists(result_handle.stdout_file().str()));
    ATF_REQUIRE(!atf::utils::file_exists(result_handle.stderr_file().str()));
    ATF_REQUIRE(!atf::utils::file_exists(result_handle.work_directory().str()));

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__fake_result);
ATF_TEST_CASE_BODY(integration__fake_result)
{
    const model::test_result fake_result(model::test_result_skipped,
                                         "Some fake details");

    model::test_cases_map test_cases;
    test_cases.insert(model::test_cases_map::value_type(
        "__fake__", model::test_case("__fake__", "ABC", fake_result)));

    const model::test_program_ptr program(new model::test_program(
        "mock", fs::path("the-program"), fs::current_path(), "the-suite",
        model::metadata_builder().build(), test_cases));

    const config::tree user_config = engine::empty_config();

    executor::executor_handle handle = executor::setup();

    (void)handle.spawn_test(program, "__fake__", user_config);

    executor::result_handle result_handle = handle.wait_any_test();
    ATF_REQUIRE_EQ(fake_result, result_handle.test_result());
    result_handle.cleanup();

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__check_requirements);
ATF_TEST_CASE_BODY(integration__check_requirements)
{
    const model::metadata metadata = model::metadata_builder()
        .add_required_config("abcde")
        .build();

    const model::test_program_ptr program = model::test_program_builder(
        "mock", fs::path("the-program"), fs::current_path(), "the-suite")
        .add_test_case("exit 12", metadata)
        .set_metadata(metadata)
        .build_ptr();

    const config::tree user_config = engine::empty_config();

    executor::executor_handle handle = executor::setup();

    (void)handle.spawn_test(program, "exit 12", user_config);

    executor::result_handle result_handle = handle.wait_any_test();
    ATF_REQUIRE_EQ(model::test_result(
                       model::test_result_skipped,
                       "Required configuration property 'abcde' not defined"),
                   result_handle.test_result());
    result_handle.cleanup();

    handle.cleanup();
}


ATF_TEST_CASE(integration__timeouts);
ATF_TEST_CASE_HEAD(integration__timeouts)
{
    set_md_var("timeout", "60");
}
ATF_TEST_CASE_BODY(integration__timeouts)
{
    const model::metadata metadata_timeout_2 = model::metadata_builder()
        .set_timeout(datetime::delta(2, 0)).build();
    const model::metadata metadata_timeout_5 = model::metadata_builder()
        .set_timeout(datetime::delta(5, 0)).build();

    const model::test_program_ptr program = model::test_program_builder(
        "mock", fs::path("the-program"), fs::current_path(), "the-suite")
        .add_test_case("sleep 30", metadata_timeout_2)
        .add_test_case("sleep 40", metadata_timeout_5)
        .add_test_case("exit 15", metadata_timeout_2)
        .build_ptr();

    const config::tree user_config = engine::empty_config();

    executor::executor_handle handle = executor::setup();

    const executor::exec_handle exec_handle1 = handle.spawn_test(
        program, "sleep 30", user_config);
    const executor::exec_handle exec_handle2 = handle.spawn_test(
        program, "sleep 40", user_config);
    const executor::exec_handle exec_handle3 = handle.spawn_test(
        program, "exit 15", user_config);

    {
        executor::result_handle result_handle = handle.wait_any_test();
        ATF_REQUIRE_EQ(exec_handle3, result_handle.original_exec_handle());
        ATF_REQUIRE_EQ(model::test_result(model::test_result_passed,
                                          "Exit 15"),
                       result_handle.test_result());
        result_handle.cleanup();
    }

    {
        executor::result_handle result_handle = handle.wait_any_test();
        ATF_REQUIRE_EQ(exec_handle1, result_handle.original_exec_handle());
        const datetime::delta duration =
            result_handle.end_time() - result_handle.start_time();
        ATF_REQUIRE(duration < datetime::delta(10, 0));
        ATF_REQUIRE(duration >= datetime::delta(2, 0));
        ATF_REQUIRE_EQ(model::test_result(model::test_result_broken,
                                          "Timed out"),
                       result_handle.test_result());
        result_handle.cleanup();
    }

    {
        executor::result_handle result_handle = handle.wait_any_test();
        ATF_REQUIRE_EQ(exec_handle2, result_handle.original_exec_handle());
        const datetime::delta duration =
            result_handle.end_time() - result_handle.start_time();
        ATF_REQUIRE(duration < datetime::delta(10, 0));
        ATF_REQUIRE(duration >= datetime::delta(4, 0));
        ATF_REQUIRE_EQ(model::test_result(model::test_result_broken,
                                          "Timed out"),
                       result_handle.test_result());
        result_handle.cleanup();
    }

    handle.cleanup();
}


ATF_TEST_CASE(integration__unprivileged_user);
ATF_TEST_CASE_HEAD(integration__unprivileged_user)
{
    set_md_var("require.config", "unprivileged-user");
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(integration__unprivileged_user)
{
    const model::metadata unprivileged_metadata = model::metadata_builder()
        .set_required_user("unprivileged").build();

    const model::test_program_ptr program = model::test_program_builder(
        "mock", fs::path("the-program"), fs::current_path(), "the-suite")
        .add_test_case("dump_unprivileged_user", unprivileged_metadata)
        .build_ptr();

    config::tree user_config = engine::empty_config();
    user_config.set_string("unprivileged_user",
                           get_config_var("unprivileged-user"));

    executor::executor_handle handle = executor::setup();

    (void)handle.spawn_test(program, "dump_unprivileged_user", user_config);

    executor::result_handle result_handle = handle.wait_any_test();
    const passwd::user unprivileged_user = passwd::find_user_by_name(
        get_config_var("unprivileged-user"));
    ATF_REQUIRE(atf::utils::compare_file(
        result_handle.stdout_file().str(),
        F("UID = %s\n") % unprivileged_user.uid));
    result_handle.cleanup();

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__auto_cleanup);
ATF_TEST_CASE_BODY(integration__auto_cleanup)
{
    const model::test_program_ptr program = model::test_program_builder(
        "mock", fs::path("the-program"), fs::current_path(), "the-suite")
        .add_test_case("exit 10")
        .add_test_case("exit 20")
        .add_test_case("pause")
        .build_ptr();

    const config::tree user_config = engine::empty_config();

    std::vector< executor::exec_handle > pids;
    std::vector< fs::path > paths;
    {
        executor::executor_handle handle = executor::setup();

        pids.push_back(handle.spawn_test(program, "exit 10", user_config));
        pids.push_back(handle.spawn_test(program, "exit 20", user_config));

        // This invocation is never waited for below.  This is intentional: we
        // want the destructor to clean the "leaked" test automatically so that
        // the clean up of the parent work directory also happens correctly.
        pids.push_back(handle.spawn_test(program, "pause", user_config));

        executor::result_handle result_handle1 = handle.wait_any_test();
        paths.push_back(result_handle1.stdout_file());
        paths.push_back(result_handle1.stderr_file());
        paths.push_back(result_handle1.work_directory());

        executor::result_handle result_handle2 = handle.wait_any_test();
        paths.push_back(result_handle2.stdout_file());
        paths.push_back(result_handle2.stderr_file());
        paths.push_back(result_handle2.work_directory());
    }
    for (std::vector< executor::exec_handle >::const_iterator
             iter = pids.begin(); iter != pids.end(); ++iter) {
        // We know that the executor handles are PIDs because we are
        // unit-testing the code... but this is not a valid assumption that
        // outside code can make.
        const pid_t pid = *iter;
        ensure_dead(pid);
    }
    for (std::vector< fs::path >::const_iterator iter = paths.begin();
         iter != paths.end(); ++iter) {
        ATF_REQUIRE(!atf::utils::file_exists((*iter).str()));
    }
}


/// Ensures that interrupting an executor cleans things up correctly.
///
/// This test scenario is tricky.  We spawn a master child process that runs the
/// executor code and we send a signal to it externally.  The child process
/// spawns a bunch of tests that block indefinitely and tries to wait for their
/// results.  When the signal is received, we expect an interrupt_error to be
/// raised, which in turn should clean up all test resources and exit the master
/// child process successfully.
///
/// \param signo Signal to deliver to the executor.
static void
do_signal_handling_test(const int signo)
{
    const model::test_program_ptr program = model::test_program_builder(
        "mock", fs::path("the-program"), fs::current_path(), "the-suite")
        .add_test_case("pause")
        .build_ptr();

    const config::tree user_config = engine::empty_config();

    const pid_t pid = ::fork();
    ATF_REQUIRE(pid != -1);
    if (pid == 0) {
        static const std::size_t num_children = 3;

        optional< fs::path > root_work_directory;
        try {
            executor::executor_handle handle = executor::setup();
            root_work_directory = handle.root_work_directory();

            for (std::size_t i = 0; i < num_children; ++i) {
                (void)handle.spawn_test(program, "pause", user_config);
            }

            atf::utils::create_file("spawned.txt", "");

            for (std::size_t i = 0; i < num_children; ++i) {
                executor::result_handle result_handle = handle.wait_any_test();
                // We may never reach this point in the test, but if we do let's
                // make sure the subprocess was terminated as expected.
                if (result_handle.test_result() == model::test_result(
                        model::test_result_failed, F("Signal %s") % SIGKILL)) {
                    // OK.
                } else {
                    std::cerr << "Child exited with unexpected code: "
                              << result_handle.test_result() << '\n';
                    std::exit(EXIT_FAILURE);
                }
                result_handle.cleanup();
            }
            std::cerr << "Terminating without reception of signal\n";
            std::exit(EXIT_FAILURE);
        } catch (const signals::interrupted_error& unused_error) {
            std::cerr << "Terminating due to interrupted_error\n";
            // We never kill ourselves until spawned.txt is created, so it is
            // guaranteed that the optional root_work_directory has been
            // initialized at this point.
            if (atf::utils::file_exists(root_work_directory.get().str())) {
                // Some cleanup did not happen; error out.
                std::exit(EXIT_FAILURE);
            } else {
                std::exit(EXIT_SUCCESS);
            }
        }
        std::abort();
    }

    while (!atf::utils::file_exists("spawned.txt")) {
        // Wait for processes.
    }
    ATF_REQUIRE(::unlink("spawned.txt") != -1);
    ATF_REQUIRE(::kill(pid, signo) != -1);

    int status;
    ATF_REQUIRE(::waitpid(pid, &status, 0) != -1);
    ATF_REQUIRE(WIFEXITED(status));
    ATF_REQUIRE_EQ(EXIT_SUCCESS, WEXITSTATUS(status));
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__signal_handling);
ATF_TEST_CASE_BODY(integration__signal_handling)
{
    // This test scenario is racy so run it multiple times to have higher
    // chances of exposing problems.
    const std::size_t rounds = 20;

    for (std::size_t i = 0; i < rounds; ++i) {
        std::cout << F("Testing round %s\n") % i;
        do_signal_handling_test(SIGHUP);
        do_signal_handling_test(SIGINT);
        do_signal_handling_test(SIGTERM);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__stacktrace);
ATF_TEST_CASE_BODY(integration__stacktrace)
{
    if (!utils::unlimit_core_size())
        skip("Cannot unlimit the core file size; check limits manually");

    const model::test_program_ptr program = model::test_program_builder(
        "mock", fs::path("the-program"), fs::current_path(), "the-suite")
        .add_test_case("unknown-dumps-core").build_ptr();

    const config::tree user_config = engine::empty_config();

    executor::executor_handle handle = executor::setup();

    (void)handle.spawn_test(program, "unknown-dumps-core", user_config);

    executor::result_handle result_handle = handle.wait_any_test();
    ATF_REQUIRE_EQ(model::test_result(model::test_result_failed,
                                      F("Signal %s") % SIGABRT),
                   result_handle.test_result());
    ATF_REQUIRE(!atf::utils::grep_file("attempting to gather stack trace",
                                       result_handle.stdout_file().str()));
    ATF_REQUIRE( atf::utils::grep_file("attempting to gather stack trace",
                                       result_handle.stderr_file().str()));
    result_handle.cleanup();

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__list_files_on_failure);
ATF_TEST_CASE_BODY(integration__list_files_on_failure)
{
    const model::test_program_ptr program = model::test_program_builder(
        "mock", fs::path("the-program"), fs::current_path(), "the-suite")
        .add_test_case("create_files_and_fail").build_ptr();

    const config::tree user_config = engine::empty_config();

    executor::executor_handle handle = executor::setup();

    (void)handle.spawn_test(program, "create_files_and_fail", user_config);

    executor::result_handle result_handle = handle.wait_any_test();
    ATF_REQUIRE(!atf::utils::grep_file("Files left in work directory",
                                       result_handle.stdout_file().str()));
    ATF_REQUIRE( atf::utils::grep_file("Files left in work directory",
                                       result_handle.stderr_file().str()));
    ATF_REQUIRE(!atf::utils::grep_file("^\\.$",
                                       result_handle.stderr_file().str()));
    ATF_REQUIRE(!atf::utils::grep_file("^\\..$",
                                       result_handle.stderr_file().str()));
    ATF_REQUIRE( atf::utils::grep_file("^first file$",
                                       result_handle.stderr_file().str()));
    ATF_REQUIRE( atf::utils::grep_file("^second-file$",
                                       result_handle.stderr_file().str()));
    ATF_REQUIRE( atf::utils::grep_file("^dir1$",
                                       result_handle.stderr_file().str()));
    ATF_REQUIRE(!atf::utils::grep_file("dir2",
                                       result_handle.stderr_file().str()));
    result_handle.cleanup();

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__isolate_child_is_called);
ATF_TEST_CASE_BODY(integration__isolate_child_is_called)
{
    const model::test_program_ptr program = model::test_program_builder(
        "mock", fs::path("the-program"), fs::current_path(), "the-suite")
        .add_test_case("validate_isolation").build_ptr();

    const config::tree user_config = engine::empty_config();

    executor::executor_handle handle = executor::setup();

    utils::setenv("HOME", "fake-value");
    utils::setenv("LANG", "es_ES");
    (void)handle.spawn_test(program, "validate_isolation", user_config);

    executor::result_handle result_handle = handle.wait_any_test();
    ATF_REQUIRE_EQ(model::test_result(model::test_result_passed, "Exit 0"),
                   result_handle.test_result());
    result_handle.cleanup();

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__process_group_is_terminated);
ATF_TEST_CASE_BODY(integration__process_group_is_terminated)
{
    utils::setenv("CONTROL_DIR", fs::current_path().str());

    const model::test_program_ptr program = model::test_program_builder(
        "mock", fs::path("the-program"), fs::current_path(), "the-suite")
        .add_test_case("spawn_blocking_child").build_ptr();

    const config::tree user_config = engine::empty_config();

    executor::executor_handle handle = executor::setup();
    (void)handle.spawn_test(program, "spawn_blocking_child", user_config);

    executor::result_handle result_handle = handle.wait_any_test();
    ATF_REQUIRE_EQ(model::test_result(model::test_result_passed, "Exit 0"),
                   result_handle.test_result());
    result_handle.cleanup();

    handle.cleanup();

    if (!fs::exists(fs::path("pid")))
        fail("The pid file was not created");

    std::ifstream pidfile("pid");
    ATF_REQUIRE(pidfile);
    pid_t pid;
    pidfile >> pid;
    pidfile.close();

    ensure_dead(pid);
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__prevent_clobbering_control_files);
ATF_TEST_CASE_BODY(integration__prevent_clobbering_control_files)
{
    const model::test_program_ptr program = model::test_program_builder(
        "mock", fs::path("the-program"), fs::current_path(), "the-suite")
        .add_test_case("delete_all").build_ptr();

    const config::tree user_config = engine::empty_config();

    executor::executor_handle handle = executor::setup();

    (void)handle.spawn_test(program, "delete_all", user_config);

    executor::result_handle result_handle = handle.wait_any_test();
    ATF_REQUIRE_EQ(model::test_result(model::test_result_passed, "Exit 0"),
                   result_handle.test_result());
    result_handle.cleanup();

    handle.cleanup();
}


ATF_INIT_TEST_CASES(tcs)
{
    executor::register_interface(
        "mock", std::shared_ptr< executor::interface >(new mock_interface()));

    ATF_ADD_TEST_CASE(tcs, integration__run_one);
    ATF_ADD_TEST_CASE(tcs, integration__run_many);

    ATF_ADD_TEST_CASE(tcs, integration__parameters_and_output);
    ATF_ADD_TEST_CASE(tcs, integration__timestamps);
    ATF_ADD_TEST_CASE(tcs, integration__files);

    ATF_ADD_TEST_CASE(tcs, integration__fake_result);
    ATF_ADD_TEST_CASE(tcs, integration__check_requirements);
    ATF_ADD_TEST_CASE(tcs, integration__timeouts);
    ATF_ADD_TEST_CASE(tcs, integration__unprivileged_user);
    ATF_ADD_TEST_CASE(tcs, integration__auto_cleanup);
    ATF_ADD_TEST_CASE(tcs, integration__signal_handling);
    ATF_ADD_TEST_CASE(tcs, integration__stacktrace);
    ATF_ADD_TEST_CASE(tcs, integration__list_files_on_failure);
    ATF_ADD_TEST_CASE(tcs, integration__isolate_child_is_called);
    ATF_ADD_TEST_CASE(tcs, integration__process_group_is_terminated);
    ATF_ADD_TEST_CASE(tcs, integration__prevent_clobbering_control_files);
}
