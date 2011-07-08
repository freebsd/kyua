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

#include "engine/atf_test_case.hpp"
#include "engine/atf_test_program.hpp"
#include "engine/exceptions.hpp"
#include "engine/results.ipp"
#include "engine/runner.hpp"
#include "engine/user_files/config.hpp"
#include "engine/user_files/kyuafile.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/env.hpp"
#include "utils/passwd.hpp"
#include "utils/process/children.ipp"
#include "utils/test_utils.hpp"

namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace process = utils::process;
namespace results = engine::results;
namespace runner = engine::runner;
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
static engine::atf_test_case
make_test_case(const engine::atf_test_program& test_program, const char* name,
               const engine::properties_map& props = engine::properties_map())
{
    return engine::atf_test_case::from_properties(test_program, name, props);
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

    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(tc->get_config_var("srcdir")),
        "the-suite");

    user_files::config config = mock_config;
    config.test_suites["the-suite"]["signo"] = F("%d") % signo;
    results::result_ptr result = runner::run_test_case(
        make_test_case(test_program, "validate_signal"), config);
    validate_broken(
        (F("Premature exit: received signal %d") % signo).str().c_str(),
        result.get());
}


/// Functor for the child spawned by the interrupt test.
class interrupt_child {
    const atf::tests::tc* _test_case;
    int _signo;

public:
    /// Constructs the new functor.
    ///
    /// \param test_case_ The test case running this functor.
    explicit interrupt_child(const atf::tests::tc* test_case_,
                             const int signo_) :
        _test_case(test_case_),
        _signo(signo_)
    {
    }

    /// Executes the functor.
    void
    operator()(void)
    {
        engine::properties_map metadata;
        metadata["has.cleanup"] = "true";
        user_files::config config = mock_config;
        config.test_suites["the-suite"]["control_dir"] =
            fs::current_path().str();
        config.test_suites["the-suite"]["monitor_pid"] = F("%d") % ::getpid();
        config.test_suites["the-suite"]["signo"] = F("%d") % _signo;

        const engine::atf_test_program test_program(
            fs::path("runner_helpers"),
            fs::path(_test_case->get_config_var("srcdir")), "the-suite");

        ATF_REQUIRE_THROW(
            engine::interrupted_error,
            runner::run_test_case(make_test_case(test_program, "block_body",
                                                 metadata), config));

        std::ifstream workdir_cookie("workdir");
        ATF_REQUIRE(workdir_cookie);

        std::string workdir_str;
        ATF_REQUIRE(std::getline(workdir_cookie, workdir_str).good());
        std::cout << F("Work directory was: %s\n") % workdir_str;

        bool ok = true;

        if (fs::exists(fs::path(workdir_str))) {
            std::cout << "Work directory was not cleaned\n";
            ok = false;
        }

        if (!fs::exists(fs::path("cleanup"))) {
            std::cout << "Cleanup not executed\n";
            ok = false;
        }

        std::exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
    }
};


static void
one_interrupt_check(const atf::tests::tc* test_case, const int signo)
{
    std::auto_ptr< process::child_with_files > child =
        process::child_with_files::fork(interrupt_child(test_case, signo),
                                        fs::path("out.txt"),
                                        fs::path("err.txt"));
    const process::status status = child->wait();
    utils::cat_file("out: ", fs::path("out.txt"));
    utils::cat_file("err: ", fs::path("err.txt"));
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(EXIT_SUCCESS, status.exitstatus());
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__current_directory);
ATF_TEST_CASE_BODY(run_test_case__current_directory)
{
    const engine::atf_test_program test_program(
        fs::path("program"), fs::path("."), "unit-tests");

    ATF_REQUIRE(::symlink((fs::path(get_config_var("srcdir")) /
                           "runner_helpers").c_str(), "program") != -1);
    results::result_ptr result = runner::run_test_case(
        make_test_case(test_program, "pass"), mock_config);
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__subdirectory);
ATF_TEST_CASE_BODY(run_test_case__subdirectory)
{
    const engine::atf_test_program test_program(
        fs::path("dir2/program"), fs::path("dir1"), "unit-tests");

    ATF_REQUIRE(::mkdir("dir1", 0755) != -1);
    ATF_REQUIRE(::mkdir("dir1/dir2", 0755) != -1);
    ATF_REQUIRE(::symlink((fs::path(get_config_var("srcdir")) /
                           "runner_helpers").c_str(),
                          "dir1/dir2/program") != -1);
    results::result_ptr result = runner::run_test_case(
        make_test_case(test_program, "pass"), mock_config);
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__config_variables);
ATF_TEST_CASE_BODY(run_test_case__config_variables)
{
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "the-suite");

    user_files::config config = mock_config;
    config.test_suites["the-suite"]["control_dir"] = fs::current_path().str();
    results::result_ptr result = runner::run_test_case(
        make_test_case(test_program, "create_cookie_in_control_dir"), config);
    compare_results(results::passed(), result.get());

    if (!fs::exists(fs::path("cookie")))
        fail("The cookie was not created where we expected; the test program "
             "probably received an invalid configuration variable");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__cleanup_shares_workdir);
ATF_TEST_CASE_BODY(run_test_case__cleanup_shares_workdir)
{
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "the-suite");

    engine::properties_map metadata;
    metadata["has.cleanup"] = "true";
    user_files::config config = mock_config;
    config.test_suites["the-suite"]["control_dir"] = fs::current_path().str();
    results::result_ptr result = runner::run_test_case(
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
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "the-suite");

    engine::properties_map metadata;
    metadata["has.cleanup"] = "false";
    user_files::config config = mock_config;
    config.test_suites["the-suite"]["control-dir"] = fs::current_path().str();
    results::result_ptr result = runner::run_test_case(
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
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "the-suite");

    engine::properties_map metadata;
    metadata["has.cleanup"] = "true";
    user_files::config config = mock_config;
    config.test_suites["the-suite"]["control_dir"] = fs::current_path().str();
    results::result_ptr result = runner::run_test_case(
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
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "the-suite");

    engine::properties_map metadata;
    user_files::config config = mock_config;
    config.test_suites["the-suite"]["control_dir"] = fs::current_path().str();
    results::result_ptr result = runner::run_test_case(
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


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__isolation_env);
ATF_TEST_CASE_BODY(run_test_case__isolation_env)
{
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    utils::setenv("HOME", "foobar");
    utils::setenv("LANG", "C");
    utils::setenv("LC_ALL", "C");
    utils::setenv("LC_COLLATE", "C");
    utils::setenv("LC_CTYPE", "C");
    utils::setenv("LC_MESSAGES", "C");
    utils::setenv("LC_MONETARY", "C");
    utils::setenv("LC_NUMERIC", "C");
    utils::setenv("LC_TIME", "C");
    utils::setenv("TZ", "EST+5");
    results::result_ptr result = runner::run_test_case(
        make_test_case(test_program, "validate_env"), mock_config);
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__isolation_pgrp);
ATF_TEST_CASE_BODY(run_test_case__isolation_pgrp)
{
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    const mode_t old_umask = ::umask(0002);
    results::result_ptr result = runner::run_test_case(
        make_test_case(test_program, "validate_pgrp"), mock_config);
    compare_results(results::passed(), result.get());
    ::umask(old_umask);
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__isolation_signals);
ATF_TEST_CASE_BODY(run_test_case__isolation_signals)
{
    one_signal_test(this, SIGHUP);
    one_signal_test(this, SIGUSR2);
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__isolation_timezone);
ATF_TEST_CASE_BODY(run_test_case__isolation_timezone)
{
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    utils::setenv("TZ", "EST+5");
    results::result_ptr result = runner::run_test_case(
        make_test_case(test_program, "validate_timezone"), mock_config);
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__isolation_umask);
ATF_TEST_CASE_BODY(run_test_case__isolation_umask)
{
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    const mode_t old_umask = ::umask(0002);
    results::result_ptr result = runner::run_test_case(
        make_test_case(test_program, "validate_umask"), mock_config);
    compare_results(results::passed(), result.get());
    ::umask(old_umask);
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__isolation_workdir);
ATF_TEST_CASE_BODY(run_test_case__isolation_workdir)
{
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    results::result_ptr result = runner::run_test_case(
        make_test_case(test_program, "create_cookie_in_workdir"), mock_config);
    compare_results(results::passed(), result.get());

    if (fs::exists(fs::path("cookie")))
        fail("It seems that the test case was not executed in a separate "
             "work directory");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__allowed_architectures);
ATF_TEST_CASE_BODY(run_test_case__allowed_architectures)
{
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    engine::properties_map metadata;
    metadata["require.arch"] = "i386 x86_64";
    user_files::config config = mock_config;
    config.architecture = "powerpc";
    config.platform = "";
    results::result_ptr result = runner::run_test_case(
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
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    engine::properties_map metadata;
    metadata["require.machine"] = "i386 amd64";
    user_files::config config = mock_config;
    config.architecture = "";
    config.platform = "macppc";
    results::result_ptr result = runner::run_test_case(
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
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "the-suite");

    engine::properties_map metadata;
    metadata["require.config"] = "used-var";
    user_files::config config = mock_config;
    config.test_suites["the-suite"]["control_dir"] = fs::current_path().str();
    config.test_suites["the-suite"]["unused-var"] = "value";
    results::result_ptr result = runner::run_test_case(
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
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    engine::properties_map metadata;
    metadata["require.progs"] = "/non-existent/program";
    results::result_ptr result = runner::run_test_case(
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
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    engine::properties_map metadata;
    metadata["require.user"] = "root";
    results::result_ptr result = runner::run_test_case(
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
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    engine::properties_map metadata;
    metadata["require.user"] = "root";
    results::result_ptr result = runner::run_test_case(
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
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    engine::properties_map metadata;
    metadata["require.user"] = "unprivileged";
    user_files::config config = mock_config;
    config.unprivileged_user = utils::none;
    results::result_ptr result = runner::run_test_case(
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
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    engine::properties_map metadata;
    metadata["require.user"] = "unprivileged";
    user_files::config config = mock_config;
    config.unprivileged_user = utils::none;
    results::result_ptr result = runner::run_test_case(
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
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    engine::properties_map metadata;
    metadata["require.user"] = "unprivileged";
    user_files::config config = mock_config;
    config.unprivileged_user = passwd::find_user_by_name(get_config_var(
        "unprivileged-user"));
    results::result_ptr result = runner::run_test_case(
        make_test_case(test_program, "check_unprivileged", metadata), config);
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__timeout_body);
ATF_TEST_CASE_BODY(run_test_case__timeout_body)
{
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "the-suite");

    engine::properties_map metadata;
    metadata["timeout"] = "1";
    user_files::config config = mock_config;
    config.test_suites["the-suite"]["control_dir"] = fs::current_path().str();
    results::result_ptr result = runner::run_test_case(
        make_test_case(test_program, "timeout_body", metadata), config);
    validate_broken("Test case timed out after 1 seconds", result.get());

    if (fs::exists(fs::path("cookie")))
        fail("It seems that the test case was not killed after it timed out");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__timeout_cleanup);
ATF_TEST_CASE_BODY(run_test_case__timeout_cleanup)
{
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "the-suite");

    engine::properties_map metadata;
    metadata["has.cleanup"] = "true";
    metadata["timeout"] = "1";
    user_files::config config = mock_config;
    config.test_suites["the-suite"]["control_dir"] = fs::current_path().str();
    results::result_ptr result = runner::run_test_case(
        make_test_case(test_program, "timeout_cleanup", metadata), config);
    validate_broken("Test case cleanup timed out after 1 seconds",
                    result.get());

    if (fs::exists(fs::path("cookie")))
        fail("It seems that the test case was not killed after it timed out");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__interrupt_body__sighup);
ATF_TEST_CASE_BODY(run_test_case__interrupt_body__sighup)
{
    one_interrupt_check(this, SIGHUP);
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__interrupt_body__sigint);
ATF_TEST_CASE_BODY(run_test_case__interrupt_body__sigint)
{
    one_interrupt_check(this, SIGINT);
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__interrupt_body__sigterm);
ATF_TEST_CASE_BODY(run_test_case__interrupt_body__sigterm)
{
    one_interrupt_check(this, SIGTERM);
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__missing_results_file);
ATF_TEST_CASE_BODY(run_test_case__missing_results_file)
{
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path(get_config_var("srcdir")),
        "unit-tests");

    results::result_ptr result = runner::run_test_case(
        make_test_case(test_program, "crash"), mock_config);
    validate_broken("Premature exit: received signal", result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__missing_test_program);
ATF_TEST_CASE_BODY(run_test_case__missing_test_program)
{
    const engine::atf_test_program test_program(
        fs::path("runner_helpers"), fs::path("dir"), "unit-tests");

    ATF_REQUIRE(::symlink((fs::path(get_config_var("srcdir")) /
                           "runner_helpers").c_str(), "runner_helpers") != -1);
    ATF_REQUIRE(::mkdir("dir", 0755) != -1);
    results::result_ptr result = runner::run_test_case(
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
    ATF_ADD_TEST_CASE(tcs, run_test_case__isolation_env);
    ATF_ADD_TEST_CASE(tcs, run_test_case__isolation_pgrp);
    ATF_ADD_TEST_CASE(tcs, run_test_case__isolation_signals);
    ATF_ADD_TEST_CASE(tcs, run_test_case__isolation_timezone);
    ATF_ADD_TEST_CASE(tcs, run_test_case__isolation_umask);
    ATF_ADD_TEST_CASE(tcs, run_test_case__isolation_workdir);
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
    ATF_ADD_TEST_CASE(tcs, run_test_case__interrupt_body__sighup);
    ATF_ADD_TEST_CASE(tcs, run_test_case__interrupt_body__sigint);
    ATF_ADD_TEST_CASE(tcs, run_test_case__interrupt_body__sigterm);
    ATF_ADD_TEST_CASE(tcs, run_test_case__missing_results_file);
    ATF_ADD_TEST_CASE(tcs, run_test_case__missing_test_program);
}
