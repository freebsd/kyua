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
#include <signal.h>
}

#include <atf-c++.hpp>

#include "engine/config.hpp"
#include "engine/runner.hpp"
#include "engine/scheduler.hpp"
#include "model/metadata.hpp"
#include "model/test_program_fwd.hpp"
#include "model/test_result.hpp"
#include "utils/config/tree.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/stacktrace.hpp"

namespace config = utils::config;
namespace fs = utils::fs;
namespace runner = engine::runner;
namespace scheduler = engine::scheduler;

using utils::none;


namespace {


/// Runs one plain test program and checks its result.
///
/// \param tc Pointer to the calling test case, to obtain srcdir.
/// \param test_case_name Name of the "test case" to select from the helper
///     program.
/// \param exp_result The expected result.
/// \param user_config User-provided configuration.
/// \param check_empty_output If true, verify that the output of the test is
///     silent.  This is just a hack to implement one of the test cases; we'd
///     easily have a nicer abstraction here...
static void
run_one(const atf::tests::tc* tc, const char* test_case_name,
        const model::test_result& exp_result,
        config::tree user_config = engine::empty_config(),
        const bool check_empty_output = false)
{
    scheduler::scheduler_handle handle = scheduler::setup();

    const model::test_program_ptr program(new runner::lazy_test_program(
        "atf", fs::path("test_case_atf_helpers"),
        fs::path(tc->get_config_var("srcdir")),
        "the-suite", model::metadata_builder().build(),
        user_config, handle));

    (void)handle.spawn_test(program, test_case_name, user_config);

    scheduler::result_handle_ptr result_handle = handle.wait_any();
    const scheduler::test_result_handle* test_result_handle =
        dynamic_cast< const scheduler::test_result_handle* >(
            result_handle.get());
    atf::utils::cat_file(result_handle->stdout_file().str(), "stdout: ");
    atf::utils::cat_file(result_handle->stderr_file().str(), "stderr: ");
    ATF_REQUIRE_EQ(exp_result, test_result_handle->test_result());
    if (check_empty_output) {
        ATF_REQUIRE(atf::utils::compare_file(result_handle->stdout_file().str(),
                                             ""));
        ATF_REQUIRE(atf::utils::compare_file(result_handle->stderr_file().str(),
                                             ""));
    }
    result_handle->cleanup();
    result_handle.reset();

    handle.cleanup();
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(integration__body_only__passes);
ATF_TEST_CASE_BODY(integration__body_only__passes)
{
    const model::test_result exp_result(model::test_result_passed);
    run_one(this, "pass", exp_result);
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__body_only__crashes);
ATF_TEST_CASE_BODY(integration__body_only__crashes)
{
    if (!utils::unlimit_core_size())
        skip("Cannot unlimit the core file size; check limits manually");

    const model::test_result exp_result(
        model::test_result_broken,
        F("Premature exit; test case received signal %s (core dumped)") %
        SIGABRT);
    run_one(this, "crash", exp_result);
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__body_only__times_out);
ATF_TEST_CASE_BODY(integration__body_only__times_out)
{
    config::tree user_config = engine::empty_config();
    user_config.set_string("test_suites.the-suite.control_dir", ".");
    user_config.set_string("test_suites.the-suite.timeout", "1");

    const model::test_result exp_result(
        model::test_result_broken, "Test case body timed out");
    run_one(this, "timeout_body", exp_result, user_config);

    ATF_REQUIRE(!atf::utils::file_exists("cookie"));
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__body_only__configuration_variables);
ATF_TEST_CASE_BODY(integration__body_only__configuration_variables)
{
    config::tree user_config = engine::empty_config();
    user_config.set_string("test_suites.the-suite.first", "some value");
    user_config.set_string("test_suites.the-suite.second", "some other value");

    const model::test_result exp_result(model::test_result_passed);
    run_one(this, "check_configuration_variables", exp_result, user_config);
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__body_only__no_atf_run_warning);
ATF_TEST_CASE_BODY(integration__body_only__no_atf_run_warning)
{
    const model::test_result exp_result(model::test_result_passed);
    run_one(this, "pass", exp_result, engine::empty_config(), true);
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__body_and_cleanup__body_times_out);
ATF_TEST_CASE_BODY(integration__body_and_cleanup__body_times_out)
{
    config::tree user_config = engine::empty_config();
    user_config.set_string("test_suites.the-suite.control_dir", ".");
    user_config.set_string("test_suites.the-suite.timeout", "1");

    const model::test_result exp_result(
        model::test_result_broken, "Test case body timed out");
    run_one(this, "timeout_body", exp_result, user_config);

    ATF_REQUIRE(!atf::utils::file_exists("cookie"));
    expect_fail("Current atf interface implementation is unable to execute "
                "the cleanup of a test after its body fails");
    ATF_REQUIRE(atf::utils::file_exists("cookie.cleanup"));
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__body_and_cleanup__cleanup_crashes);
ATF_TEST_CASE_BODY(integration__body_and_cleanup__cleanup_crashes)
{
    const model::test_result exp_result(
        model::test_result_broken,
        "Test case cleanup did not terminate successfully");
    run_one(this, "crash_cleanup", exp_result);
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__body_and_cleanup__cleanup_times_out);
ATF_TEST_CASE_BODY(integration__body_and_cleanup__cleanup_times_out)
{
    config::tree user_config = engine::empty_config();
    user_config.set_string("test_suites.the-suite.control_dir", ".");
    user_config.set_string("test_suites.the-suite.timeout", "1");

    const model::test_result exp_result(
        model::test_result_broken, "Test case cleanup timed out");
    run_one(this, "timeout_cleanup", exp_result, user_config);

    ATF_REQUIRE(!atf::utils::file_exists("cookie"));
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__body_and_cleanup__expect_timeout);
ATF_TEST_CASE_BODY(integration__body_and_cleanup__expect_timeout)
{
    config::tree user_config = engine::empty_config();
    user_config.set_string("test_suites.the-suite.control_dir", ".");
    user_config.set_string("test_suites.the-suite.timeout", "1");

    const model::test_result exp_result(
        model::test_result_expected_failure, "Times out on purpose");
    run_one(this, "expect_timeout", exp_result, user_config);

    ATF_REQUIRE(!atf::utils::file_exists("cookie"));
    expect_fail("Current atf interface implementation is unable to execute "
                "the cleanup of a test after its body fails");
    ATF_REQUIRE(atf::utils::file_exists("cookie.cleanup"));
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__body_and_cleanup__shared_workdir);
ATF_TEST_CASE_BODY(integration__body_and_cleanup__shared_workdir)
{
    const model::test_result exp_result(model::test_result_passed);
    run_one(this, "shared_workdir", exp_result);
}


ATF_INIT_TEST_CASES(tcs)
{
    scheduler::register_interface(
        "atf", std::shared_ptr< scheduler::interface >(
            new engine::atf_interface()));

    ATF_ADD_TEST_CASE(tcs, integration__body_only__passes);
    ATF_ADD_TEST_CASE(tcs, integration__body_only__crashes);
    ATF_ADD_TEST_CASE(tcs, integration__body_only__times_out);
    ATF_ADD_TEST_CASE(tcs, integration__body_only__configuration_variables);
    ATF_ADD_TEST_CASE(tcs, integration__body_only__no_atf_run_warning);
    ATF_ADD_TEST_CASE(tcs, integration__body_and_cleanup__body_times_out);
    ATF_ADD_TEST_CASE(tcs, integration__body_and_cleanup__cleanup_crashes);
    ATF_ADD_TEST_CASE(tcs, integration__body_and_cleanup__cleanup_times_out);
    ATF_ADD_TEST_CASE(tcs, integration__body_and_cleanup__expect_timeout);
    ATF_ADD_TEST_CASE(tcs, integration__body_and_cleanup__shared_workdir);
}
