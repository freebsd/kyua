// Copyright 2011 Google Inc.
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

#include <cerrno>
#include <fstream>
#include <iostream>

#include <atf-c++.hpp>

#include "engine/exceptions.hpp"
#include "engine/plain_iface/test_case.hpp"
#include "engine/plain_iface/test_program.hpp"
#include "engine/user_files/config.hpp"
#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/format/macros.hpp"
#include "utils/process/children.ipp"
#include "utils/sanity.hpp"
#include "utils/test_utils.hpp"

namespace fs = utils::fs;
namespace plain_iface = engine::plain_iface;
namespace process = utils::process;
namespace results = engine::results;
namespace user_files = engine::user_files;


namespace {


/// Simplifies the execution of the helper test cases.
class plain_helper {
    const atf::tests::tc* _atf_tc;
    fs::path _binary_path;
    fs::path _root;

public:
    /// Constructs a new helper.
    ///
    /// \param atf_tc A pointer to the calling test case.  Needed to obtain
    ///     run-time configuration variables.
    /// \param name The name of the helper to run.
    plain_helper(const atf::tests::tc* atf_tc, const char* name) :
        _atf_tc(atf_tc),
        _binary_path("test_case_helpers"),
        _root(atf_tc->get_config_var("srcdir"))
    {
        utils::setenv("TEST_CASE", name);
    }

    /// Sets an environment variable for the helper.
    ///
    /// This is simply syntactic sugar for utils::setenv.
    ///
    /// \param variable The name of the environment variable to set.
    /// \param value The value of the variable; must be convertible to a string.
    template< typename T >
    void
    set(const char* variable, const T& value)
    {
        utils::setenv(variable, F("%s") % value);
    }

    /// Places the helper in a different location.
    ///
    /// This prepares the helper to be run from a different location than the
    /// source directory so that the runtime execution can be validated.
    ///
    /// \param new_binary_path The new path to the binary, relative to the test
    ///     suite root.
    /// \param new_root The new test suite root.
    ///
    /// \pre The directory holding the target test program must exist.
    ///     Otherwise, the relocation of the binary will fail.
    void
    move(const char* new_binary_path, const char* new_root)
    {
        _binary_path = fs::path(new_binary_path);
        _root = fs::path(new_root);

        const fs::path src_path = fs::path(_atf_tc->get_config_var("srcdir")) /
            "test_case_helpers";
        const fs::path new_path = _root / _binary_path;
        ATF_REQUIRE(
            ::symlink(src_path.c_str(), new_path.c_str()) != -1);
    }

    /// Runs the helper.
    ///
    /// \param config The runtime engine configuration, if different to the
    /// defaults.
    results::result_ptr
    run(const user_files::config& config = user_files::config::defaults()) const
    {
        const plain_iface::test_program test_program(_binary_path, _root,
                                                     "unit-tests");
        const plain_iface::test_case test_case(test_program);
        return test_case.run(config);
    }
};


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

    plain_helper helper(tc, "validate_signal");
    helper.set("SIGNO", signo);
    const results::result_ptr result = helper.run();
    validate_broken(
        (F("Received signal %d") % signo).str().c_str(), result.get());
}


/// Functor for the child spawned by the interrupt test.
class interrupt_child {
    const atf::tests::tc* _atf_tc;
    int _signo;

public:
    /// Constructs the new functor.
    ///
    /// \param test_case_ The test case running this functor.
    explicit interrupt_child(const atf::tests::tc* atf_tc_,
                             const int signo_) :
        _atf_tc(atf_tc_),
        _signo(signo_)
    {
    }

    /// Executes the functor.
    void
    operator()(void)
    {
        plain_helper helper(_atf_tc, "block");
        helper.set("CONTROL_DIR", fs::current_path());
        helper.set("MONITOR_PID", ::getpid());
        helper.set("SIGNO", _signo);
        ATF_REQUIRE_THROW(engine::interrupted_error, helper.run());

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


ATF_TEST_CASE_WITHOUT_HEAD(ctor);
ATF_TEST_CASE_BODY(ctor)
{
    const plain_iface::test_program test_program(fs::path("program"),
                                                 fs::path("root"),
                                                 "test-suite");
    const plain_iface::test_case test_case(test_program);
    ATF_REQUIRE(&test_program == &test_case.test_program());
    ATF_REQUIRE_EQ("main", test_case.name());
}


ATF_TEST_CASE_WITHOUT_HEAD(all_properties);
ATF_TEST_CASE_BODY(all_properties)
{
    const plain_iface::test_program test_program(fs::path("program"),
                                                 fs::path("root"),
                                                 "test-suite");
    const plain_iface::test_case test_case(test_program);
    ATF_REQUIRE(test_case.all_properties().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(run__result_pass);
ATF_TEST_CASE_BODY(run__result_pass)
{
    const results::result_ptr result = plain_helper(this, "pass").run();
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run__result_fail);
ATF_TEST_CASE_BODY(run__result_fail)
{
    const results::result_ptr result = plain_helper(this, "fail").run();
    compare_results(results::failed("Exited with code 8"), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run__result_crash);
ATF_TEST_CASE_BODY(run__result_crash)
{
    const results::result_ptr result = plain_helper(this, "crash").run();
    validate_broken("Received signal 6", result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run__current_directory);
ATF_TEST_CASE_BODY(run__current_directory)
{
    plain_helper helper(this, "pass");
    helper.move("program", ".");
    results::result_ptr result = helper.run();
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run__subdirectory);
ATF_TEST_CASE_BODY(run__subdirectory)
{
    plain_helper helper(this, "pass");
    ATF_REQUIRE(::mkdir("dir1", 0755) != -1);
    ATF_REQUIRE(::mkdir("dir1/dir2", 0755) != -1);
    helper.move("dir2/program", "dir1");
    results::result_ptr result = helper.run();
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run__kill_children);
ATF_TEST_CASE_BODY(run__kill_children)
{
    plain_helper helper(this, "spawn_blocking_child");
    helper.set("CONTROL_DIR", fs::current_path());
    const results::result_ptr result = helper.run();
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


ATF_TEST_CASE_WITHOUT_HEAD(run__isolation_env);
ATF_TEST_CASE_BODY(run__isolation_env)
{
    const plain_helper helper(this, "validate_env");
    utils::setenv("TEST_CASE", "validate_env");
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
    const results::result_ptr result = helper.run();
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run__isolation_pgrp);
ATF_TEST_CASE_BODY(run__isolation_pgrp)
{
    const plain_helper helper(this, "validate_pgrp");
    const results::result_ptr result = helper.run();
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run__isolation_signals);
ATF_TEST_CASE_BODY(run__isolation_signals)
{
    one_signal_test(this, SIGHUP);
    one_signal_test(this, SIGUSR2);
}


ATF_TEST_CASE_WITHOUT_HEAD(run__isolation_timezone);
ATF_TEST_CASE_BODY(run__isolation_timezone)
{
    const plain_helper helper(this, "validate_timezone");
    const results::result_ptr result = helper.run();
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run__isolation_umask);
ATF_TEST_CASE_BODY(run__isolation_umask)
{
    const plain_helper helper(this, "validate_umask");
    const mode_t old_umask = ::umask(0002);
    const results::result_ptr result = helper.run();
    compare_results(results::passed(), result.get());
    ::umask(old_umask);
}


ATF_TEST_CASE_WITHOUT_HEAD(run__isolation_workdir);
ATF_TEST_CASE_BODY(run__isolation_workdir)
{
    const plain_helper helper(this, "create_cookie_in_workdir");
    const results::result_ptr result = helper.run();
    compare_results(results::passed(), result.get());

    if (fs::exists(fs::path("cookie")))
        fail("It seems that the test case was not executed in a separate "
             "work directory");
}


#if 0
// This needs the TODO regarding the parametrization of the timeout in test_case
// fixed first.
ATF_TEST_CASE_WITHOUT_HEAD(run__timeout);
ATF_TEST_CASE_BODY(run__timeout)
{
    const plain_iface::test_program test_program(
        fs::path("test_case_helpers"), fs::path(get_config_var("srcdir")),
        "the-suite");

    //engine::properties_map metadata;
    //metadata["timeout"] = "1";
    utils::setenv("TEST_CASE", "timeout");
    utils::setenv("CONTROL_DIR", fs::current_path().str());
    results::result_ptr result = plain_iface::test_case(
        test_program).run(user_files::config::defaults());
    validate_broken("Test case timed out", result.get());

    if (fs::exists(fs::path("cookie")))
        fail("It seems that the test case was not killed after it timed out");
}
#endif


ATF_TEST_CASE_WITHOUT_HEAD(run__interrupt_body__sighup);
ATF_TEST_CASE_BODY(run__interrupt_body__sighup)
{
    one_interrupt_check(this, SIGHUP);
}


ATF_TEST_CASE_WITHOUT_HEAD(run__interrupt_body__sigint);
ATF_TEST_CASE_BODY(run__interrupt_body__sigint)
{
    one_interrupt_check(this, SIGINT);
}


ATF_TEST_CASE_WITHOUT_HEAD(run__interrupt_body__sigterm);
ATF_TEST_CASE_BODY(run__interrupt_body__sigterm)
{
    one_interrupt_check(this, SIGTERM);
}


ATF_TEST_CASE_WITHOUT_HEAD(run__missing_test_program);
ATF_TEST_CASE_BODY(run__missing_test_program)
{
    plain_helper helper(this, "pass");
    ATF_REQUIRE(::mkdir("dir", 0755) != -1);
    helper.move("test_case_helpers", "dir");
    ATF_REQUIRE(::unlink("dir/test_case_helpers") != -1);
    const results::result_ptr result = helper.run();
    validate_broken("Failed to execute", result.get());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, ctor);
    ATF_ADD_TEST_CASE(tcs, all_properties);

    ATF_ADD_TEST_CASE(tcs, run__result_pass);
    ATF_ADD_TEST_CASE(tcs, run__result_fail);
    ATF_ADD_TEST_CASE(tcs, run__result_crash);
    ATF_ADD_TEST_CASE(tcs, run__current_directory);
    ATF_ADD_TEST_CASE(tcs, run__subdirectory);
    ATF_ADD_TEST_CASE(tcs, run__kill_children);
    ATF_ADD_TEST_CASE(tcs, run__isolation_env);
    ATF_ADD_TEST_CASE(tcs, run__isolation_pgrp);
    ATF_ADD_TEST_CASE(tcs, run__isolation_signals);
    ATF_ADD_TEST_CASE(tcs, run__isolation_timezone);
    ATF_ADD_TEST_CASE(tcs, run__isolation_umask);
    ATF_ADD_TEST_CASE(tcs, run__isolation_workdir);
    //ATF_ADD_TEST_CASE(tcs, run__timeout);
    ATF_ADD_TEST_CASE(tcs, run__interrupt_body__sighup);
    ATF_ADD_TEST_CASE(tcs, run__interrupt_body__sigint);
    ATF_ADD_TEST_CASE(tcs, run__interrupt_body__sigterm);
    ATF_ADD_TEST_CASE(tcs, run__missing_test_program);
}
