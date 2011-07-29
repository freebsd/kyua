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
#include <typeinfo>

#include <atf-c++.hpp>

#include "engine/atf_iface/test_case.hpp"
#include "engine/atf_iface/runner.hpp"
#include "engine/atf_iface/test_program.hpp"
#include "engine/exceptions.hpp"
#include "engine/results.ipp"
#include "engine/user_files/config.hpp"
#include "engine/user_files/kyuafile.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/env.hpp"
#include "utils/passwd.hpp"
#include "utils/process/children.ipp"
#include "utils/test_utils.hpp"

namespace atf_iface = engine::atf_iface;
namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace process = utils::process;
namespace results = engine::results;
namespace user_files = engine::user_files;


namespace {


/// Fake configuration.
static const user_files::config mock_config(
    "mock-architecture", "mock-platform", utils::none,
    user_files::test_suites_map());


/// Compares two test results and fails the test case if they differ.
///
/// TODO(jmmv): This is a verbatim duplicate from results_test.cpp.  Move to a
/// separate test_utils module, just as was done in the utils/ subdirectory.
///
/// \param expected The expected result.
/// \param actual A pointer to the actual result.
template< class Result >
static void
compare_results(const Result& expected, const results::base_result* actual)
{
    std::cout << F("Result is of type '%s'\n") % typeid(*actual).name();

    if (typeid(*actual) == typeid(results::broken)) {
        const results::broken* broken = dynamic_cast< const results::broken* >(
            actual);
        ATF_FAIL(F("Got unexpected broken result: %s") % broken->reason);
    } else {
        if (typeid(*actual) != typeid(expected)) {
            ATF_FAIL(F("Result %s does not match type %s") %
                     typeid(*actual).name() % typeid(expected).name());
        } else {
            const Result* actual_typed = dynamic_cast< const Result* >(actual);
            ATF_REQUIRE(expected == *actual_typed);
        }
    }
}


/// Validates a broken test case and fails the test case if invalid.
///
/// TODO(jmmv): This is a verbatim duplicate from results_test.cpp.  Move to a
/// separate test_utils module, just as was done in the utils/ subdirectory.
///
/// \param reason_regexp The reason to match against the broken reason.
/// \param actual A pointer to the actual result.
static void
validate_broken(const char* reason_regexp, const results::base_result* actual)
{
    std::cout << F("Result is of type '%s'\n") % typeid(*actual).name();

    if (typeid(*actual) == typeid(results::broken)) {
        const results::broken* broken = dynamic_cast< const results::broken* >(
            actual);
        std::cout << F("Got reason: %s\n") % broken->reason;
        ATF_REQUIRE_MATCH(reason_regexp, broken->reason);
    } else {
        ATF_FAIL(F("Expected broken result but got %s") %
                 typeid(*actual).name());
    }
}


/// Instantiates a test case.
///
/// \param path The test program.
/// \param name The test case name.
/// \param props The raw properties to pass to the test case.
///
/// \return The new test case.
static atf_iface::test_case
make_test_case(const atf_iface::test_program& test_program, const char* name,
               const engine::properties_map& props = engine::properties_map())
{
    return atf_iface::test_case::from_properties(test_program, name, props);
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__current_directory);
ATF_TEST_CASE_BODY(run_test_case__current_directory)
{
    const atf_iface::test_program test_program(
        fs::path("program"), fs::path("."), "unit-tests");

    ATF_REQUIRE(::symlink((fs::path(get_config_var("srcdir")) /
                           "runner_helpers").c_str(), "program") != -1);
    results::result_ptr result = atf_iface::run_test_case(
        make_test_case(test_program, "pass"), mock_config);
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__subdirectory);
ATF_TEST_CASE_BODY(run_test_case__subdirectory)
{
    const atf_iface::test_program test_program(
        fs::path("dir2/program"), fs::path("dir1"), "unit-tests");

    ATF_REQUIRE(::mkdir("dir1", 0755) != -1);
    ATF_REQUIRE(::mkdir("dir1/dir2", 0755) != -1);
    ATF_REQUIRE(::symlink((fs::path(get_config_var("srcdir")) /
                           "runner_helpers").c_str(),
                          "dir1/dir2/program") != -1);
    results::result_ptr result = atf_iface::run_test_case(
        make_test_case(test_program, "pass"), mock_config);
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__config_variables);
ATF_TEST_CASE_BODY(run_test_case__config_variables)
{
    const atf_iface::test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "the-suite");

    user_files::config config = mock_config;
    config.test_suites["the-suite"]["control_dir"] = fs::current_path().str();
    results::result_ptr result = atf_iface::run_test_case(
        make_test_case(test_program, "create_cookie_in_control_dir"), config);
    compare_results(results::passed(), result.get());

    if (!fs::exists(fs::path("cookie")))
        fail("The cookie was not created where we expected; the test program "
             "probably received an invalid configuration variable");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__cleanup_shares_workdir);
ATF_TEST_CASE_BODY(run_test_case__cleanup_shares_workdir)
{
    const atf_iface::test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "the-suite");

    engine::properties_map metadata;
    metadata["has.cleanup"] = "true";
    user_files::config config = mock_config;
    config.test_suites["the-suite"]["control_dir"] = fs::current_path().str();
    results::result_ptr result = atf_iface::run_test_case(
        make_test_case(test_program, "check_cleanup_workdir", metadata),
        config);
    compare_results(results::skipped("cookie created"), result.get());

    if (fs::exists(fs::path("missing_cookie")))
        fail("The cleanup part did not see the cookie; the work directory "
             "is probably not shared");
    if (fs::exists(fs::path("invalid_cookie")))
        fail("The cleanup part read an invalid cookie");
    if (!fs::exists(fs::path("cookie_ok")))
        fail("The cleanup part was not executed");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__has_cleanup__false);
ATF_TEST_CASE_BODY(run_test_case__has_cleanup__false)
{
    const atf_iface::test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "the-suite");

    engine::properties_map metadata;
    metadata["has.cleanup"] = "false";
    user_files::config config = mock_config;
    config.test_suites["the-suite"]["control-dir"] = fs::current_path().str();
    results::result_ptr result = atf_iface::run_test_case(
        make_test_case(test_program, "create_cookie_from_cleanup", metadata),
        config);
    compare_results(results::passed(), result.get());

    if (fs::exists(fs::path("cookie")))
        fail("The cleanup part was executed even though the test case set "
             "has.cleanup to false");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__has_cleanup__true);
ATF_TEST_CASE_BODY(run_test_case__has_cleanup__true)
{
    const atf_iface::test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "the-suite");

    engine::properties_map metadata;
    metadata["has.cleanup"] = "true";
    user_files::config config = mock_config;
    config.test_suites["the-suite"]["control_dir"] = fs::current_path().str();
    results::result_ptr result = atf_iface::run_test_case(
        make_test_case(test_program, "create_cookie_from_cleanup", metadata),
        config);
    compare_results(results::passed(), result.get());

    if (!fs::exists(fs::path("cookie")))
        fail("The cleanup part was not executed even though the test case set "
             "has.cleanup to true");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__kill_children);
ATF_TEST_CASE_BODY(run_test_case__kill_children)
{
    const atf_iface::test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "the-suite");

    engine::properties_map metadata;
    user_files::config config = mock_config;
    config.test_suites["the-suite"]["control_dir"] = fs::current_path().str();
    results::result_ptr result = atf_iface::run_test_case(
        make_test_case(test_program, "spawn_blocking_child", metadata), config);
    compare_results(results::passed(), result.get());

    if (!fs::exists(fs::path("pid")))
        fail("The pid file was not created");
    std::ifstream pidfile("pid");
    ATF_REQUIRE(pidfile);
    pid_t pid;
    pidfile >> pid;
    pidfile.close();

    if (::kill(pid, SIGCONT) != -1 || errno != ESRCH) {
        // Looks like the subchild did not die.  Note that this might be
        // inaccurate: the system may have spawned a new process with the same
        // pid as our subchild... but in practice, this does not happen because
        // most systems do not immediately reuse pid numbers.
        fail(F("The subprocess %d of our child was not killed") % pid);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__isolation);
ATF_TEST_CASE_BODY(run_test_case__isolation)
{
    const atf_iface::test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    // Simple checks to make sure that isolate_process has been called.
    utils::setenv("HOME", "foobar");
    utils::setenv("LANG", "C");
    results::result_ptr result = atf_iface::run_test_case(
        make_test_case(test_program, "validate_isolation"), mock_config);
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__allowed_architectures);
ATF_TEST_CASE_BODY(run_test_case__allowed_architectures)
{
    const atf_iface::test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    engine::properties_map metadata;
    metadata["require.arch"] = "i386 x86_64";
    user_files::config config = mock_config;
    config.architecture = "powerpc";
    config.platform = "";
    results::result_ptr result = atf_iface::run_test_case(
        make_test_case(test_program, "create_cookie_in_control_dir", metadata),
        config);
    compare_results(results::skipped(
       "Current architecture 'powerpc' not supported"),
        result.get());

    if (fs::exists(fs::path("cookie")))
        fail("The test case was not really skipped when the requirements "
             "check failed");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__allowed_platforms);
ATF_TEST_CASE_BODY(run_test_case__allowed_platforms)
{
    const atf_iface::test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    engine::properties_map metadata;
    metadata["require.machine"] = "i386 amd64";
    user_files::config config = mock_config;
    config.architecture = "";
    config.platform = "macppc";
    results::result_ptr result = atf_iface::run_test_case(
        make_test_case(test_program, "create_cookie_in_control_dir", metadata),
        config);
    compare_results(results::skipped(
       "Current platform 'macppc' not supported"),
        result.get());

    if (fs::exists(fs::path("cookie")))
        fail("The test case was not really skipped when the requirements "
             "check failed");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__required_configs);
ATF_TEST_CASE_BODY(run_test_case__required_configs)
{
    const atf_iface::test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "the-suite");

    engine::properties_map metadata;
    metadata["require.config"] = "used-var";
    user_files::config config = mock_config;
    config.test_suites["the-suite"]["control_dir"] = fs::current_path().str();
    config.test_suites["the-suite"]["unused-var"] = "value";
    results::result_ptr result = atf_iface::run_test_case(
        make_test_case(test_program, "create_cookie_in_control_dir", metadata),
        config);
    compare_results(results::skipped(
        "Required configuration property 'used-var' not defined"),
        result.get());

    if (fs::exists(fs::path("cookie")))
        fail("The test case was not really skipped when the requirements "
             "check failed");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__required_programs);
ATF_TEST_CASE_BODY(run_test_case__required_programs)
{
    const atf_iface::test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    engine::properties_map metadata;
    metadata["require.progs"] = "/non-existent/program";
    results::result_ptr result = atf_iface::run_test_case(
        make_test_case(test_program, "create_cookie_in_control_dir", metadata),
        mock_config);
    compare_results(results::skipped(
        "Required program '/non-existent/program' not found"), result.get());

    if (fs::exists(fs::path("cookie")))
        fail("The test case was not really skipped when the requirements "
             "check failed");
}


ATF_TEST_CASE(run_test_case__required_user__root__ok);
ATF_TEST_CASE_HEAD(run_test_case__required_user__root__ok)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(run_test_case__required_user__root__ok)
{
    const atf_iface::test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    engine::properties_map metadata;
    metadata["require.user"] = "root";
    results::result_ptr result = atf_iface::run_test_case(
        make_test_case(test_program, "create_cookie_in_workdir", metadata),
        mock_config);
    ATF_REQUIRE(passwd::current_user().is_root());
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE(run_test_case__required_user__root__skip);
ATF_TEST_CASE_HEAD(run_test_case__required_user__root__skip)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(run_test_case__required_user__root__skip)
{
    const atf_iface::test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    engine::properties_map metadata;
    metadata["require.user"] = "root";
    results::result_ptr result = atf_iface::run_test_case(
        make_test_case(test_program, "create_cookie_in_workdir", metadata),
        mock_config);
    ATF_REQUIRE(!passwd::current_user().is_root());
    compare_results(results::skipped("Requires root privileges"), result.get());
}


ATF_TEST_CASE(run_test_case__required_user__unprivileged__ok);
ATF_TEST_CASE_HEAD(run_test_case__required_user__unprivileged__ok)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(run_test_case__required_user__unprivileged__ok)
{
    const atf_iface::test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    engine::properties_map metadata;
    metadata["require.user"] = "unprivileged";
    user_files::config config = mock_config;
    config.unprivileged_user = utils::none;
    results::result_ptr result = atf_iface::run_test_case(
        make_test_case(test_program, "create_cookie_in_workdir", metadata),
        config);
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE(run_test_case__required_user__unprivileged__skip);
ATF_TEST_CASE_HEAD(run_test_case__required_user__unprivileged__skip)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(run_test_case__required_user__unprivileged__skip)
{
    const atf_iface::test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    engine::properties_map metadata;
    metadata["require.user"] = "unprivileged";
    user_files::config config = mock_config;
    config.unprivileged_user = utils::none;
    results::result_ptr result = atf_iface::run_test_case(
        make_test_case(test_program, "create_cookie_in_workdir", metadata),
        config);
    compare_results(results::skipped(
        "Requires an unprivileged user but the unprivileged-user "
        "configuration variable is not defined"), result.get());
}


ATF_TEST_CASE(run_test_case__required_user__unprivileged__drop);
ATF_TEST_CASE_HEAD(run_test_case__required_user__unprivileged__drop)
{
    set_md_var("require.config", "unprivileged-user");
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(run_test_case__required_user__unprivileged__drop)
{
    const atf_iface::test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    engine::properties_map metadata;
    metadata["require.user"] = "unprivileged";
    user_files::config config = mock_config;
    config.unprivileged_user = passwd::find_user_by_name(get_config_var(
        "unprivileged-user"));
    results::result_ptr result = atf_iface::run_test_case(
        make_test_case(test_program, "check_unprivileged", metadata), config);
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__timeout_body);
ATF_TEST_CASE_BODY(run_test_case__timeout_body)
{
    const atf_iface::test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "the-suite");

    engine::properties_map metadata;
    metadata["timeout"] = "1";
    user_files::config config = mock_config;
    config.test_suites["the-suite"]["control_dir"] = fs::current_path().str();
    results::result_ptr result = atf_iface::run_test_case(
        make_test_case(test_program, "timeout_body", metadata), config);
    validate_broken("Test case body timed out", result.get());

    if (fs::exists(fs::path("cookie")))
        fail("It seems that the test case was not killed after it timed out");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__timeout_cleanup);
ATF_TEST_CASE_BODY(run_test_case__timeout_cleanup)
{
    const atf_iface::test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "the-suite");

    engine::properties_map metadata;
    metadata["has.cleanup"] = "true";
    metadata["timeout"] = "1";
    user_files::config config = mock_config;
    config.test_suites["the-suite"]["control_dir"] = fs::current_path().str();
    results::result_ptr result = atf_iface::run_test_case(
        make_test_case(test_program, "timeout_cleanup", metadata), config);
    validate_broken("Test case cleanup timed out", result.get());

    if (fs::exists(fs::path("cookie")))
        fail("It seems that the test case was not killed after it timed out");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__missing_results_file);
ATF_TEST_CASE_BODY(run_test_case__missing_results_file)
{
    const atf_iface::test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    results::result_ptr result = atf_iface::run_test_case(
        make_test_case(test_program, "crash"), mock_config);
    validate_broken("Premature exit: received signal", result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__missing_test_program);
ATF_TEST_CASE_BODY(run_test_case__missing_test_program)
{
    const atf_iface::test_program test_program(
        fs::path("runner_helpers"), fs::path("dir"), "unit-tests");

    ATF_REQUIRE(::symlink((fs::path(get_config_var("srcdir")) /
                           "runner_helpers").c_str(), "runner_helpers") != -1);
    ATF_REQUIRE(::mkdir("dir", 0755) != -1);
    results::result_ptr result = atf_iface::run_test_case(
        make_test_case(test_program, "passed"), mock_config);
    validate_broken("Failed to execute", result.get());
}


// TODO(jmmv): Implement tests to validate that the stdout/stderr of the test
// case body and cleanup are correctly captured by run_test_case.  We probably
// have to wait until we have a mechanism to store this data to do so.


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, run_test_case__current_directory);
    ATF_ADD_TEST_CASE(tcs, run_test_case__subdirectory);
    ATF_ADD_TEST_CASE(tcs, run_test_case__config_variables);
    ATF_ADD_TEST_CASE(tcs, run_test_case__cleanup_shares_workdir);
    ATF_ADD_TEST_CASE(tcs, run_test_case__has_cleanup__false);
    ATF_ADD_TEST_CASE(tcs, run_test_case__has_cleanup__true);
    ATF_ADD_TEST_CASE(tcs, run_test_case__kill_children);
    ATF_ADD_TEST_CASE(tcs, run_test_case__isolation);
    ATF_ADD_TEST_CASE(tcs, run_test_case__allowed_architectures);
    ATF_ADD_TEST_CASE(tcs, run_test_case__allowed_platforms);
    ATF_ADD_TEST_CASE(tcs, run_test_case__required_configs);
    ATF_ADD_TEST_CASE(tcs, run_test_case__required_programs);
    ATF_ADD_TEST_CASE(tcs, run_test_case__required_user__root__ok);
    ATF_ADD_TEST_CASE(tcs, run_test_case__required_user__root__skip);
    ATF_ADD_TEST_CASE(tcs, run_test_case__required_user__unprivileged__ok);
    ATF_ADD_TEST_CASE(tcs, run_test_case__required_user__unprivileged__skip);
    ATF_ADD_TEST_CASE(tcs, run_test_case__required_user__unprivileged__drop);
    ATF_ADD_TEST_CASE(tcs, run_test_case__timeout_body);
    ATF_ADD_TEST_CASE(tcs, run_test_case__timeout_cleanup);
    ATF_ADD_TEST_CASE(tcs, run_test_case__missing_results_file);
    ATF_ADD_TEST_CASE(tcs, run_test_case__missing_test_program);
}
