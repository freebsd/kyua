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
}

#include <iostream>
#include <stdexcept>

#include <atf-c++.hpp>

#include "engine/results.ipp"
#include "engine/runner.hpp"
#include "engine/test_case.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/env.hpp"
#include "utils/noncopyable.hpp"
#include "utils/passwd.hpp"

namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace results = engine::results;
namespace runner = engine::runner;


namespace {


/// Mapping between test case identifier to their results.
typedef std::map< engine::test_case_id, const results::base_result* >
    results_map;


/// Callbacks for the execution of test suites and programs.
class capture_results : public runner::hooks, utils::noncopyable {
public:
    results_map results;

    ~capture_results(void)
    {
        for (results_map::const_iterator iter = results.begin();
             iter != results.end(); iter++) {
            delete (*iter).second;
        }
    }

    void
    start_test_case(const engine::test_case_id& identifier)
    {
        results[identifier] = NULL;
    }

    void
    finish_test_case(const engine::test_case_id& identifier,
                     results::result_ptr result)
    {
        if (results.find(identifier) == results.end())
            ATF_FAIL(F("finish_test_case called with id %s but start_test_case "
                       "was never called") % identifier.str());
        else
            results[identifier] = result.release();
    }
};


/// Gets the path to the runtime helpers.
///
/// \param tc A pointer to the current test case, to query the 'srcdir'
///     variable.
///
/// \return The path to the helpers.
static fs::path
get_helpers_path(const atf::tests::tc* tc)
{
    return fs::path(tc->get_config_var("srcdir")) / "runner_helpers";
}


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


/// Program a signal to be ignored.
///
/// If the programming fails, this terminates the test case.  After the handler
/// is installed, this also delivers a signal to the caller process to ensure
/// that the signal is effectively being ignored -- otherwise we probably crash,
/// which would report the test case as broken.
///
/// \param signo The signal to be ignored.
static void
ignore_signal(const int signo)
{
    struct ::sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    ATF_REQUIRE(::sigaction(signo, &sa, NULL) != -1);

    ::kill(::getpid(), signo);
}


/// Instantiates a test case.
///
/// \param path The test program.
/// \param name The test case name.
/// \param props The raw properties to pass to the test case.
///
/// \return The new test case.
static engine::test_case
make_test_case(const fs::path& path, const char* name,
               const engine::properties_map& props = engine::properties_map())
{
    const engine::test_case_id id(path, name);
    return engine::test_case::from_properties(id, props);
}


/// Ensures that a signal handler is resetted in the test case.
///
/// \param tc A pointer to the caller test case.
/// \param signo The signal to test.
static void
one_signal_test(const atf::tests::tc* tc, const int signo)
{
    PRE_MSG(signo != SIGKILL && signo != SIGSTOP, "The signal to test must be "
            "programmable");

    ignore_signal(signo);

    engine::properties_map user_config;
    user_config["signo"] = F("%d") % signo;
    results::result_ptr result = runner::run_test_case(
        make_test_case(get_helpers_path(tc), "validate_signal"),
        engine::config(), user_config);
    validate_broken("Results file.*cannot be opened", result.get());
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__simple);
ATF_TEST_CASE_BODY(run_test_case__simple)
{
    results::result_ptr result = runner::run_test_case(
        make_test_case(get_helpers_path(this), "pass"),
        engine::config(), engine::properties_map());
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__config_variables);
ATF_TEST_CASE_BODY(run_test_case__config_variables)
{
    engine::properties_map user_config;
    user_config["control_dir"] = fs::current_path().str();
    results::result_ptr result = runner::run_test_case(
        make_test_case(get_helpers_path(this), "create_cookie_in_control_dir"),
        engine::config(), user_config);
    compare_results(results::passed(), result.get());

    if (!fs::exists(fs::path("cookie")))
        fail("The cookie was not created where we expected; the test program "
             "probably received an invalid configuration variable");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__cleanup_shares_workdir);
ATF_TEST_CASE_BODY(run_test_case__cleanup_shares_workdir)
{
    engine::properties_map metadata;
    metadata["has.cleanup"] = "true";
    engine::properties_map user_config;
    user_config["control_dir"] = fs::current_path().str();
    results::result_ptr result = runner::run_test_case(
        make_test_case(get_helpers_path(this), "check_cleanup_workdir",
                       metadata),
        engine::config(), user_config);
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
    engine::properties_map metadata;
    metadata["has.cleanup"] = "false";
    engine::properties_map user_config;
    user_config["control_dir"] = fs::current_path().str();
    results::result_ptr result = runner::run_test_case(
        make_test_case(get_helpers_path(this), "create_cookie_from_cleanup",
                       metadata), engine::config(), user_config);
    compare_results(results::passed(), result.get());

    if (fs::exists(fs::path("cookie")))
        fail("The cleanup part was executed even though the test case set "
             "has.cleanup to false");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__has_cleanup__true);
ATF_TEST_CASE_BODY(run_test_case__has_cleanup__true)
{
    engine::properties_map metadata;
    metadata["has.cleanup"] = "true";
    engine::properties_map user_config;
    user_config["control_dir"] = fs::current_path().str();
    results::result_ptr result = runner::run_test_case(
        make_test_case(get_helpers_path(this), "create_cookie_from_cleanup",
                       metadata), engine::config(), user_config);
    compare_results(results::passed(), result.get());

    if (!fs::exists(fs::path("cookie")))
        fail("The cleanup part was not executed even though the test case set "
             "has.cleanup to true");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__isolation_env);
ATF_TEST_CASE_BODY(run_test_case__isolation_env)
{
    utils::setenv("HOME", "foobar");
    utils::setenv("LANG", "C");
    utils::setenv("LC_ALL", "C");
    utils::setenv("LC_COLLATE", "C");
    utils::setenv("LC_CTYPE", "C");
    utils::setenv("LC_MESSAGES", "C");
    utils::setenv("LC_MONETARY", "C");
    utils::setenv("LC_NUMERIC", "C");
    utils::setenv("LC_TIME", "C");
    utils::setenv("TZ", "C");
    results::result_ptr result = runner::run_test_case(
        make_test_case(get_helpers_path(this), "validate_env"),
        engine::config(), engine::properties_map());
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__isolation_pgrp);
ATF_TEST_CASE_BODY(run_test_case__isolation_pgrp)
{
    const mode_t old_umask = ::umask(0002);
    results::result_ptr result = runner::run_test_case(
        make_test_case(get_helpers_path(this), "validate_pgrp"),
        engine::config(), engine::properties_map());
    compare_results(results::passed(), result.get());
    ::umask(old_umask);
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__isolation_signals);
ATF_TEST_CASE_BODY(run_test_case__isolation_signals)
{
    one_signal_test(this, SIGHUP);
    one_signal_test(this, SIGUSR2);
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__isolation_umask);
ATF_TEST_CASE_BODY(run_test_case__isolation_umask)
{
    const mode_t old_umask = ::umask(0002);
    results::result_ptr result = runner::run_test_case(
        make_test_case(get_helpers_path(this), "validate_umask"),
        engine::config(), engine::properties_map());
    compare_results(results::passed(), result.get());
    ::umask(old_umask);
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__isolation_workdir);
ATF_TEST_CASE_BODY(run_test_case__isolation_workdir)
{
    results::result_ptr result = runner::run_test_case(
        make_test_case(get_helpers_path(this), "create_cookie_in_workdir"),
        engine::config(), engine::properties_map());
    compare_results(results::passed(), result.get());

    if (fs::exists(fs::path("cookie")))
        fail("It seems that the test case was not executed in a separate "
             "work directory");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__allowed_architectures);
ATF_TEST_CASE_BODY(run_test_case__allowed_architectures)
{
    engine::properties_map metadata;
    metadata["require.arch"] = "i386 x86_64";
    engine::config config;
    config.architecture = "powerpc";
    config.platform = "";
    results::result_ptr result = runner::run_test_case(
        make_test_case(get_helpers_path(this), "create_cookie_in_control_dir",
                       metadata),
        config, engine::properties_map());
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
    engine::properties_map metadata;
    metadata["require.machine"] = "i386 amd64";
    engine::config config;
    config.architecture = "";
    config.platform = "macppc";
    results::result_ptr result = runner::run_test_case(
        make_test_case(get_helpers_path(this), "create_cookie_in_control_dir",
                       metadata),
        config, engine::properties_map());
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
    engine::properties_map metadata;
    metadata["require.config"] = "used-var";
    engine::properties_map user_config;
    user_config["control_dir"] = fs::current_path().str();
    user_config["unused-var"] = "value";
    results::result_ptr result = runner::run_test_case(
        make_test_case(get_helpers_path(this), "create_cookie_in_control_dir",
                       metadata),
        engine::config(), user_config);
    compare_results(results::skipped(
        "Required configuration property 'used-var' not defined"),
        result.get());

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
    engine::properties_map metadata;
    metadata["require.user"] = "root";
    engine::config config;
    results::result_ptr result = runner::run_test_case(
        make_test_case(get_helpers_path(this), "create_cookie_in_workdir",
                       metadata), config, engine::properties_map());
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
    engine::properties_map metadata;
    metadata["require.user"] = "root";
    engine::config config;
    results::result_ptr result = runner::run_test_case(
        make_test_case(get_helpers_path(this), "create_cookie_in_workdir",
                       metadata), config, engine::properties_map());
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
    engine::properties_map metadata;
    metadata["require.user"] = "unprivileged";
    engine::config config;
    config.unprivileged_user = utils::none;
    results::result_ptr result = runner::run_test_case(
        make_test_case(get_helpers_path(this), "create_cookie_in_workdir",
                       metadata), config, engine::properties_map());
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE(run_test_case__required_user__unprivileged__skip);
ATF_TEST_CASE_HEAD(run_test_case__required_user__unprivileged__skip)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(run_test_case__required_user__unprivileged__skip)
{
    engine::properties_map metadata;
    metadata["require.user"] = "unprivileged";
    engine::config config;
    config.unprivileged_user = utils::none;
    results::result_ptr result = runner::run_test_case(
        make_test_case(get_helpers_path(this), "create_cookie_in_workdir",
                       metadata), config, engine::properties_map());
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
    engine::properties_map metadata;
    metadata["require.user"] = "unprivileged";
    engine::config config;
    config.unprivileged_user = passwd::find_user_by_name(get_config_var(
        "unprivileged-user"));
    results::result_ptr result = runner::run_test_case(
        make_test_case(get_helpers_path(this), "check_unprivileged",
                       metadata), config, engine::properties_map());
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__timeout_body);
ATF_TEST_CASE_BODY(run_test_case__timeout_body)
{
    engine::properties_map metadata;
    metadata["timeout"] = "1";
    engine::properties_map user_config;
    user_config["control_dir"] = fs::current_path().str();
    results::result_ptr result = runner::run_test_case(
        make_test_case(get_helpers_path(this), "timeout_body", metadata),
        engine::config(), user_config);
    validate_broken("Test case timed out after 1 seconds", result.get());

    if (fs::exists(fs::path("cookie")))
        fail("It seems that the test case was not killed after it timed out");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__timeout_cleanup);
ATF_TEST_CASE_BODY(run_test_case__timeout_cleanup)
{
    engine::properties_map metadata;
    metadata["has.cleanup"] = "true";
    metadata["timeout"] = "1";
    engine::properties_map user_config;
    user_config["control_dir"] = fs::current_path().str();
    results::result_ptr result = runner::run_test_case(
        make_test_case(get_helpers_path(this), "timeout_cleanup", metadata),
        engine::config(), user_config);
    validate_broken("Test case cleanup timed out after 1 seconds",
                    result.get());

    if (fs::exists(fs::path("cookie")))
        fail("It seems that the test case was not killed after it timed out");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__missing_results_file);
ATF_TEST_CASE_BODY(run_test_case__missing_results_file)
{
    results::result_ptr result = runner::run_test_case(
        make_test_case(get_helpers_path(this), "crash"),
        engine::config(), engine::properties_map());
    // TODO(jmmv): This should really contain a more descriptive message.
    validate_broken("Results file.*cannot be opened", result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__missing_test_program);
ATF_TEST_CASE_BODY(run_test_case__missing_test_program)
{
    results::result_ptr result = runner::run_test_case(
        make_test_case(fs::path("/non-existent"), "passed"),
        engine::config(), engine::properties_map());
    // TODO(jmmv): This should really be either an exception to denote a broken
    // test suite or should be properly reported as missing test program.
    validate_broken("Results file.*cannot be opened", result.get());
}


// TODO(jmmv): Implement tests to validate that the stdout/stderr of the test
// case body and cleanup are correctly captured by run_test_case.  We probably
// have to wait until we have a mechanism to store this data to do so.


// TODO(jmmv): Need more test cases for run_test_program and run_test_suite.


ATF_TEST_CASE_WITHOUT_HEAD(run_test_program__load_failure);
ATF_TEST_CASE_BODY(run_test_program__load_failure)
{
    capture_results hooks;
    runner::run_test_program(fs::path("/non-existent"),
                             engine::properties_map(), &hooks);
    const engine::test_case_id id(fs::path("/non-existent"),
                                  "__test_program__");
    ATF_REQUIRE(hooks.results.find(id) != hooks.results.end());
    const results::base_result* result = hooks.results[id];
    validate_broken("Failed to load list of test cases", result);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, run_test_case__simple);
    ATF_ADD_TEST_CASE(tcs, run_test_case__config_variables);
    ATF_ADD_TEST_CASE(tcs, run_test_case__cleanup_shares_workdir);
    ATF_ADD_TEST_CASE(tcs, run_test_case__has_cleanup__false);
    ATF_ADD_TEST_CASE(tcs, run_test_case__has_cleanup__true);
    ATF_ADD_TEST_CASE(tcs, run_test_case__isolation_env);
    ATF_ADD_TEST_CASE(tcs, run_test_case__isolation_pgrp);
    ATF_ADD_TEST_CASE(tcs, run_test_case__isolation_signals);
    ATF_ADD_TEST_CASE(tcs, run_test_case__isolation_umask);
    ATF_ADD_TEST_CASE(tcs, run_test_case__isolation_workdir);
    ATF_ADD_TEST_CASE(tcs, run_test_case__allowed_architectures);
    ATF_ADD_TEST_CASE(tcs, run_test_case__allowed_platforms);
    ATF_ADD_TEST_CASE(tcs, run_test_case__required_configs);
    ATF_ADD_TEST_CASE(tcs, run_test_case__required_user__root__ok);
    ATF_ADD_TEST_CASE(tcs, run_test_case__required_user__root__skip);
    ATF_ADD_TEST_CASE(tcs, run_test_case__required_user__unprivileged__ok);
    ATF_ADD_TEST_CASE(tcs, run_test_case__required_user__unprivileged__skip);
    ATF_ADD_TEST_CASE(tcs, run_test_case__required_user__unprivileged__drop);
    ATF_ADD_TEST_CASE(tcs, run_test_case__timeout_body);
    ATF_ADD_TEST_CASE(tcs, run_test_case__timeout_cleanup);
    ATF_ADD_TEST_CASE(tcs, run_test_case__missing_results_file);
    ATF_ADD_TEST_CASE(tcs, run_test_case__missing_test_program);

    ATF_ADD_TEST_CASE(tcs, run_test_program__load_failure);
}
