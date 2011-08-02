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
#include "utils/optional.ipp"
#include "utils/process/children.ipp"
#include "utils/sanity.hpp"
#include "utils/test_utils.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace plain_iface = engine::plain_iface;
namespace process = utils::process;
namespace results = engine::results;
namespace user_files = engine::user_files;

using utils::none;
using utils::optional;


namespace {


/// Simplifies the execution of the helper test cases.
class plain_helper {
    const atf::tests::tc* _atf_tc;
    fs::path _binary_path;
    fs::path _root;
    optional< datetime::delta > _timeout;

public:
    /// Constructs a new helper.
    ///
    /// \param atf_tc A pointer to the calling test case.  Needed to obtain
    ///     run-time configuration variables.
    /// \param name The name of the helper to run.
    plain_helper(const atf::tests::tc* atf_tc, const char* name,
                 const optional< datetime::delta > timeout = none) :
        _atf_tc(atf_tc),
        _binary_path("test_case_helpers"),
        _root(atf_tc->get_config_var("srcdir")),
        _timeout(timeout)
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
                                                     "unit-tests", _timeout);
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


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(ctor);
ATF_TEST_CASE_BODY(ctor)
{
    const plain_iface::test_program test_program(fs::path("program"),
                                                 fs::path("root"),
                                                 "test-suite", none);
    const plain_iface::test_case test_case(test_program);
    ATF_REQUIRE(&test_program == &test_case.test_program());
    ATF_REQUIRE_EQ("main", test_case.name());
}


ATF_TEST_CASE_WITHOUT_HEAD(all_properties);
ATF_TEST_CASE_BODY(all_properties)
{
    const plain_iface::test_program test_program(fs::path("program"),
                                                 fs::path("root"),
                                                 "test-suite", none);
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


ATF_TEST_CASE_WITHOUT_HEAD(run__isolation);
ATF_TEST_CASE_BODY(run__isolation)
{
    const plain_helper helper(this, "validate_isolation");
    utils::setenv("TEST_CASE", "validate_isolation");
    // Simple checks to make sure that isolate_process has been called.
    utils::setenv("HOME", "foobar");
    utils::setenv("LANG", "C");
    const results::result_ptr result = helper.run();
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run__timeout);
ATF_TEST_CASE_BODY(run__timeout)
{
    plain_helper helper(this, "timeout",
                        utils::make_optional(datetime::delta(1, 0)));
    helper.set("CONTROL_DIR", fs::current_path());
    const results::result_ptr result = helper.run();
    validate_broken("Test case timed out", result.get());

    if (fs::exists(fs::path("cookie")))
        fail("It seems that the test case was not killed after it timed out");
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
    ATF_ADD_TEST_CASE(tcs, run__isolation);
    ATF_ADD_TEST_CASE(tcs, run__timeout);
    ATF_ADD_TEST_CASE(tcs, run__missing_test_program);
}
