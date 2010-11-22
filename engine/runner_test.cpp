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

#include <iostream>
#include <stdexcept>

#include <atf-c++.hpp>

#include "engine/results.ipp"
#include "engine/runner.hpp"
#include "engine/test_case.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/noncopyable.hpp"
#include "utils/test_utils.hpp"

namespace fs = utils::fs;
namespace results = engine::results;
namespace runner = engine::runner;


namespace {


/// Mapping between test case identifier to their results.
typedef std::map< std::string, const results::base_result* > results_map;


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
    start_test_case(const utils::fs::path& test_program,
                    const std::string& test_case)
    {
        const std::string id = F("%s:%s") % test_program.str() % test_case;
        results[id] = NULL;
    }

    void
    finish_test_case(const utils::fs::path& test_program,
                     const std::string& test_case,
                     std::auto_ptr< const results::base_result > result)
    {
        const std::string id = F("%s:%s") % test_program.str() % test_case;
        if (results.find(id) == results.end())
            ATF_FAIL(F("finish_test_case called with id %s but start_test_case "
                       "was never called") % id);
        else
            results[id] = result.release();
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


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__simple);
ATF_TEST_CASE_BODY(run_test_case__simple)
{
    const engine::test_case test_case(get_helpers_path(this), "pass",
                                      engine::properties_map());
    std::auto_ptr< const results::base_result > result = runner::run_test_case(
        test_case, engine::properties_map());
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__separate_workdir);
ATF_TEST_CASE_BODY(run_test_case__separate_workdir)
{
    const engine::test_case test_case(get_helpers_path(this),
                                      "create_cookie_in_workdir",
                                      engine::properties_map());
    std::auto_ptr< const results::base_result > result = runner::run_test_case(
        test_case, engine::properties_map());
    compare_results(results::passed(), result.get());

    if (utils::exists(fs::path("cookie")))
        fail("It seems that the test case was not executed in a separate "
             "work directory");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__config_variables);
ATF_TEST_CASE_BODY(run_test_case__config_variables)
{
    const engine::test_case test_case(get_helpers_path(this),
                                      "create_cookie_in_control_dir",
                                      engine::properties_map());
    engine::properties_map config;
    config["control_dir"] = fs::current_path().str();
    std::auto_ptr< const results::base_result > result = runner::run_test_case(
        test_case, config);
    compare_results(results::passed(), result.get());

    if (!utils::exists(fs::path("cookie")))
        fail("The cookie was not created where we expected; the test program "
             "probably received an invalid configuration variable");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__cleanup_shares_workdir);
ATF_TEST_CASE_BODY(run_test_case__cleanup_shares_workdir)
{
    const engine::test_case test_case(get_helpers_path(this),
                                      "check_cleanup_workdir",
                                      engine::properties_map());
    engine::properties_map config;
    config["control_dir"] = fs::current_path().str();
    std::auto_ptr< const results::base_result > result = runner::run_test_case(
        test_case, config);
    compare_results(results::skipped("cookie created"), result.get());

    if (utils::exists(fs::path("missing_cookie")))
        fail("The cleanup part did not see the cookie; the work directory "
             "is probably not shared");
    if (utils::exists(fs::path("invalid_cookie")))
        fail("The cleanup part read an invalid cookie");
    if (!utils::exists(fs::path("cookie_ok")))
        fail("The cleanup part was not executed");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__missing_results_file);
ATF_TEST_CASE_BODY(run_test_case__missing_results_file)
{
    const engine::test_case test_case(get_helpers_path(this), "crash",
                                      engine::properties_map());
    std::auto_ptr< const results::base_result > result = runner::run_test_case(
        test_case, engine::properties_map());
    // TODO(jmmv): This should really be contain a more descriptive message.
    validate_broken("Results file.*cannot be opened", result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__missing_test_program);
ATF_TEST_CASE_BODY(run_test_case__missing_test_program)
{
    const engine::test_case test_case(fs::path("/non-existent"), "passed",
                                      engine::properties_map());
    std::auto_ptr< const results::base_result > result = runner::run_test_case(
        test_case, engine::properties_map());
    // TODO(jmmv): This should really be either an exception to denote a broken
    // test suite or should be properly reported as missing test program.
    validate_broken("Results file.*cannot be opened", result.get());
}


// TODO(jmmv): Implement tests to validate that the stdout/stderr of the test
// case body and cleanup are correctly captures by run_test_case.  We probably
// have to wait until we have a mechanism to store this data to do so.
// Also implement tests to validate test case isolation.


// TODO(jmmv): Need more test cases for run_test_program and run_test_suite.


ATF_TEST_CASE_WITHOUT_HEAD(run_test_program__load_failure);
ATF_TEST_CASE_BODY(run_test_program__load_failure)
{
    capture_results hooks;
    runner::run_test_program(fs::path("/non-existent"),
                             engine::properties_map(), &hooks);
    ATF_REQUIRE(hooks.results.find("/non-existent:__test_program__") !=
                hooks.results.end());
    const results::base_result* result = hooks.results["/non-existent:"
                                                       "__test_program__"];
    validate_broken("Failed to load list of test cases", result);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, run_test_case__simple);
    ATF_ADD_TEST_CASE(tcs, run_test_case__config_variables);
    ATF_ADD_TEST_CASE(tcs, run_test_case__cleanup_shares_workdir);
    ATF_ADD_TEST_CASE(tcs, run_test_case__missing_results_file);
    ATF_ADD_TEST_CASE(tcs, run_test_case__missing_test_program);

    ATF_ADD_TEST_CASE(tcs, run_test_program__load_failure);
}
