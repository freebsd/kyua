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

#include "engine/scheduler.hpp"

extern "C" {
#include <sys/types.h>

#include <signal.h>
#include <unistd.h>
}

#include <cstdlib>
#include <fstream>
#include <iostream>

#include <atf-c++.hpp>

#include "engine/config.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "utils/config/tree.ipp"
#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/process/status.hpp"
#include "utils/sanity.hpp"
#include "utils/stacktrace.hpp"
#include "utils/text/exceptions.hpp"
#include "utils/text/operations.ipp"

namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace process = utils::process;
namespace scheduler = engine::scheduler;
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
/// This scheduler interface does not execute external binaries.  It is designed
/// to simulate the scheduler of various programs with different exit statuses.
class mock_interface : public scheduler::interface {
    /// Executes the subprocess simulating an exec.
    ///
    /// This is just a simple wrapper over _exit(2) because we cannot use
    /// std::exit on exit from this mock interface.  The reason is that we do
    /// not want to invoke any destructors as otherwise we'd clear up the global
    /// scheduler state by mistake.  This wouldn't be a major problem if it
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
    /// separate from any control files that the scheduler may have created.
    void
    exec_delete_all(void) const UTILS_NORETURN
    {
        const int exit_code = ::system("rm *") == -1
            ? EXIT_FAILURE : EXIT_SUCCESS;

        // Recreate our own cookie.
        atf::utils::create_file("exec_test_was_called", "");

        do_exit(exit_code);
    }

    /// Executes a test case that returns a specific exit code.
    ///
    /// \param exit_code Exit status to terminate the program with.
    void
    exec_exit(const int exit_code) const UTILS_NORETURN
    {
        do_exit(exit_code);
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
                      const config::properties_map& vars) const
        UTILS_NORETURN
    {
        std::cout << F("Test program: %s\n") % test_program.relative_path();
        std::cout << F("Test case: %s\n") % test_case_name;
        for (config::properties_map::const_iterator iter = vars.begin();
             iter != vars.end(); ++iter) {
            std::cout << F("%s=%s\n") % (*iter).first % (*iter).second;
        }

        std::cerr << F("stderr: %s\n") % test_case_name;

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
              const config::properties_map& vars,
              const fs::path& control_directory) const
    {
        const fs::path cookie = control_directory / "exec_test_was_called";
        std::ofstream control_file(cookie.c_str());
        if (!control_file) {
            std::cerr << "Failed to create " << cookie << '\n';
            std::abort();
        }

        if (starts_with(test_case_name, "create_files_and_fail")) {
            exec_create_files_and_fail();
        } else if (test_case_name == "delete_all") {
            exec_delete_all();
        } else if (starts_with(test_case_name, "exit ")) {
            exec_exit(suffix_to_int(test_case_name, "exit "));
        } else if (starts_with(test_case_name, "print_params")) {
            exec_print_params(test_program, test_case_name, vars);
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


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(integration__run_one);
ATF_TEST_CASE_BODY(integration__run_one)
{
    const model::test_program_ptr program = model::test_program_builder(
        "mock", fs::path("the-program"), fs::current_path(), "the-suite")
        .add_test_case("exit 41").build_ptr();

    const config::tree user_config = engine::empty_config();

    scheduler::scheduler_handle handle = scheduler::setup();

    const scheduler::exec_handle exec_handle = handle.spawn_test(
        program, "exit 41", user_config);

    scheduler::result_handle_ptr result_handle = handle.wait_any();
    const scheduler::test_result_handle* test_result_handle =
        dynamic_cast< const scheduler::test_result_handle* >(
            result_handle.get());
    ATF_REQUIRE_EQ(exec_handle, result_handle->original_exec_handle());
    ATF_REQUIRE_EQ(model::test_result(model::test_result_passed, "Exit 41"),
                   test_result_handle->test_result());
    result_handle->cleanup();
    result_handle.reset();

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__run_many);
ATF_TEST_CASE_BODY(integration__run_many)
{
    static const std::size_t num_test_programs = 30;

    const config::tree user_config = engine::empty_config();

    scheduler::scheduler_handle handle = scheduler::setup();

    // We mess around with the "current time" below, so make sure the tests do
    // not spuriously exceed their deadline by bumping it to a large number.
    const model::metadata infinite_timeout = model::metadata_builder()
        .set_timeout(datetime::delta(1000000L, 0)).build();

    std::size_t total_tests = 0;
    std::map< scheduler::exec_handle, model::test_program_ptr >
        exp_test_programs;
    std::map< scheduler::exec_handle, std::string > exp_test_case_names;
    std::map< scheduler::exec_handle, datetime::timestamp > exp_start_times;
    std::map< scheduler::exec_handle, int > exp_exit_statuses;
    for (std::size_t i = 0; i < num_test_programs; ++i) {
        const std::string test_case_0 = F("exit %s") % (i * 3 + 0);
        const std::string test_case_1 = F("exit %s") % (i * 3 + 1);
        const std::string test_case_2 = F("exit %s") % (i * 3 + 2);

        const model::test_program_ptr program = model::test_program_builder(
            "mock", fs::path(F("program-%s") % i),
            fs::current_path(), "the-suite")
            .set_metadata(infinite_timeout)
            .add_test_case(test_case_0)
            .add_test_case(test_case_1)
            .add_test_case(test_case_2)
            .build_ptr();

        const datetime::timestamp start_time = datetime::timestamp::from_values(
            2014, 12, 8, 9, 40, 0, i);

        scheduler::exec_handle exec_handle;

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
        scheduler::result_handle_ptr result_handle = handle.wait_any();
        const scheduler::test_result_handle* test_result_handle =
            dynamic_cast< const scheduler::test_result_handle* >(
                result_handle.get());

        const scheduler::exec_handle exec_handle =
            result_handle->original_exec_handle();

        const model::test_program_ptr test_program = exp_test_programs.find(
            exec_handle)->second;
        const std::string& test_case_name = exp_test_case_names.find(
            exec_handle)->second;
        const datetime::timestamp& start_time = exp_start_times.find(
            exec_handle)->second;
        const int exit_status = exp_exit_statuses.find(exec_handle)->second;

        ATF_REQUIRE_EQ(model::test_result(model::test_result_passed,
                                          F("Exit %s") % exit_status),
                       test_result_handle->test_result());

        ATF_REQUIRE_EQ(test_program, test_result_handle->test_program());
        ATF_REQUIRE_EQ(test_case_name, test_result_handle->test_case_name());

        ATF_REQUIRE_EQ(start_time, result_handle->start_time());
        ATF_REQUIRE_EQ(end_time, result_handle->end_time());

        result_handle->cleanup();

        ATF_REQUIRE(!atf::utils::file_exists(
                        result_handle->stdout_file().str()));
        ATF_REQUIRE(!atf::utils::file_exists(
                        result_handle->stderr_file().str()));
        ATF_REQUIRE(!atf::utils::file_exists(
                        result_handle->work_directory().str()));

        result_handle.reset();
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

    scheduler::scheduler_handle handle = scheduler::setup();

    const scheduler::exec_handle exec_handle = handle.spawn_test(
        program, "print_params", user_config);

    scheduler::result_handle_ptr result_handle = handle.wait_any();
    const scheduler::test_result_handle* test_result_handle =
        dynamic_cast< const scheduler::test_result_handle* >(
            result_handle.get());

    ATF_REQUIRE_EQ(exec_handle, result_handle->original_exec_handle());
    ATF_REQUIRE_EQ(program, test_result_handle->test_program());
    ATF_REQUIRE_EQ("print_params", test_result_handle->test_case_name());
    ATF_REQUIRE_EQ(model::test_result(model::test_result_passed, "Exit 0"),
                   test_result_handle->test_result());

    ATF_REQUIRE(atf::utils::compare_file(
        result_handle->stdout_file().str(),
        "Test program: the-program\n"
        "Test case: print_params\n"
        "one=first variable\n"
        "two=second variable\n"));
    ATF_REQUIRE(atf::utils::compare_file(
        result_handle->stderr_file().str(), "stderr: print_params\n"));

    result_handle->cleanup();
    result_handle.reset();

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

    scheduler::scheduler_handle handle = scheduler::setup();

    (void)handle.spawn_test(program, "__fake__", user_config);

    scheduler::result_handle_ptr result_handle = handle.wait_any();
    const scheduler::test_result_handle* test_result_handle =
        dynamic_cast< const scheduler::test_result_handle* >(
            result_handle.get());
    ATF_REQUIRE_EQ(fake_result, test_result_handle->test_result());
    result_handle->cleanup();
    result_handle.reset();

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__check_requirements);
ATF_TEST_CASE_BODY(integration__check_requirements)
{
    const model::test_program_ptr program = model::test_program_builder(
        "mock", fs::path("the-program"), fs::current_path(), "the-suite")
        .add_test_case("exit 12")
        .set_metadata(model::metadata_builder()
                      .add_required_config("abcde").build())
        .build_ptr();

    const config::tree user_config = engine::empty_config();

    scheduler::scheduler_handle handle = scheduler::setup();

    (void)handle.spawn_test(program, "exit 12", user_config);

    scheduler::result_handle_ptr result_handle = handle.wait_any();
    const scheduler::test_result_handle* test_result_handle =
        dynamic_cast< const scheduler::test_result_handle* >(
            result_handle.get());
    ATF_REQUIRE_EQ(model::test_result(
                       model::test_result_skipped,
                       "Required configuration property 'abcde' not defined"),
                   test_result_handle->test_result());
    result_handle->cleanup();
    result_handle.reset();

    handle.cleanup();
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

    scheduler::scheduler_handle handle = scheduler::setup();

    (void)handle.spawn_test(program, "unknown-dumps-core", user_config);

    scheduler::result_handle_ptr result_handle = handle.wait_any();
    const scheduler::test_result_handle* test_result_handle =
        dynamic_cast< const scheduler::test_result_handle* >(
            result_handle.get());
    ATF_REQUIRE_EQ(model::test_result(model::test_result_failed,
                                      F("Signal %s") % SIGABRT),
                   test_result_handle->test_result());
    ATF_REQUIRE(!atf::utils::grep_file("attempting to gather stack trace",
                                       result_handle->stdout_file().str()));
    ATF_REQUIRE( atf::utils::grep_file("attempting to gather stack trace",
                                       result_handle->stderr_file().str()));
    result_handle->cleanup();
    result_handle.reset();

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__list_files_on_failure);
ATF_TEST_CASE_BODY(integration__list_files_on_failure)
{
    const model::test_program_ptr program = model::test_program_builder(
        "mock", fs::path("the-program"), fs::current_path(), "the-suite")
        .add_test_case("create_files_and_fail").build_ptr();

    const config::tree user_config = engine::empty_config();

    scheduler::scheduler_handle handle = scheduler::setup();

    (void)handle.spawn_test(program, "create_files_and_fail", user_config);

    scheduler::result_handle_ptr result_handle = handle.wait_any();
    ATF_REQUIRE(!atf::utils::grep_file("Files left in work directory",
                                       result_handle->stdout_file().str()));
    ATF_REQUIRE( atf::utils::grep_file("Files left in work directory",
                                       result_handle->stderr_file().str()));
    ATF_REQUIRE(!atf::utils::grep_file("^\\.$",
                                       result_handle->stderr_file().str()));
    ATF_REQUIRE(!atf::utils::grep_file("^\\..$",
                                       result_handle->stderr_file().str()));
    ATF_REQUIRE( atf::utils::grep_file("^first file$",
                                       result_handle->stderr_file().str()));
    ATF_REQUIRE( atf::utils::grep_file("^second-file$",
                                       result_handle->stderr_file().str()));
    ATF_REQUIRE( atf::utils::grep_file("^dir1$",
                                       result_handle->stderr_file().str()));
    ATF_REQUIRE(!atf::utils::grep_file("dir2",
                                       result_handle->stderr_file().str()));
    result_handle->cleanup();
    result_handle.reset();

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__prevent_clobbering_control_files);
ATF_TEST_CASE_BODY(integration__prevent_clobbering_control_files)
{
    const model::test_program_ptr program = model::test_program_builder(
        "mock", fs::path("the-program"), fs::current_path(), "the-suite")
        .add_test_case("delete_all").build_ptr();

    const config::tree user_config = engine::empty_config();

    scheduler::scheduler_handle handle = scheduler::setup();

    (void)handle.spawn_test(program, "delete_all", user_config);

    scheduler::result_handle_ptr result_handle = handle.wait_any();
    const scheduler::test_result_handle* test_result_handle =
        dynamic_cast< const scheduler::test_result_handle* >(
            result_handle.get());
    ATF_REQUIRE_EQ(model::test_result(model::test_result_passed, "Exit 0"),
                   test_result_handle->test_result());
    result_handle->cleanup();
    result_handle.reset();

    handle.cleanup();
}


ATF_INIT_TEST_CASES(tcs)
{
    scheduler::register_interface(
        "mock", std::shared_ptr< scheduler::interface >(new mock_interface()));

    ATF_ADD_TEST_CASE(tcs, integration__run_one);
    ATF_ADD_TEST_CASE(tcs, integration__run_many);

    ATF_ADD_TEST_CASE(tcs, integration__parameters_and_output);

    ATF_ADD_TEST_CASE(tcs, integration__fake_result);
    ATF_ADD_TEST_CASE(tcs, integration__check_requirements);
    ATF_ADD_TEST_CASE(tcs, integration__stacktrace);
    ATF_ADD_TEST_CASE(tcs, integration__list_files_on_failure);
    ATF_ADD_TEST_CASE(tcs, integration__prevent_clobbering_control_files);
}
