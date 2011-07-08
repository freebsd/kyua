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
#include <signal.h>
}

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <typeinfo>

#include <atf-c++.hpp>

#include "engine/atf_test_case.hpp"
#include "engine/atf_test_program.hpp"
#include "engine/exceptions.hpp"
#include "engine/results.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/process/children.ipp"
#include "utils/test_utils.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace process = utils::process;
namespace results = engine::results;

using utils::none;
using utils::optional;


namespace {


/// Functor to execute a helper test case.
///
/// This is supposed to be used as a fork() hook to execute a test case in a
/// subprocess.
class run_helpers {
    fs::path _srcdir;
    std::string _test_case;
    fs::path _resfile;

public:
    /// Constructs a new functor.
    ///
    /// \param tc A pointer to the caller test case; required to get 'srcdir'.
    /// \param test_case The name of the helper test case to execute.
    /// \param resfile The name of the results file.
    run_helpers(const atf::tests::tc* tc, const std::string& test_case,
                const fs::path& resfile) :
        _srcdir(tc->get_config_var("srcdir")),
        _test_case(test_case),
        _resfile(resfile)
    {
    }

    /// Entry point for the functor.
    void
    operator()(void)
    {
        std::vector< std::string > args;
        args.push_back("-r" + _resfile.str());
        args.push_back(_test_case);
        process::exec(_srcdir / "results_helpers", args);
    }
};


/// Ad-hoc function to run a simple test case.
///
/// This routine does not implement all the necessary run-time isolation
/// functionality required to execute test cases.  It does just enough to be
/// able to execute one of our simple helper test cases and store the result in
/// a file for later processing.
///
/// \param tc A pointer to the caller test case; required to get 'srcdir'.
/// \param test_case The name of the helper test case to execute.
/// \param resfile The name of the results file.
static void
run_test_case(const atf::tests::tc* tc, const std::string& test_case,
              const fs::path& resfile)
{
    std::auto_ptr< process::child_with_files > child =
        process::child_with_files::fork(run_helpers(tc, test_case, resfile),
                                        fs::path("so.txt"), fs::path("se.txt"));
    (void)child->wait();
    utils::cat_file("STDOUT: ", fs::path("so.txt"));
    utils::cat_file("STDERR: ", fs::path("se.txt"));
    utils::cat_file("RESULT: ", resfile);
}


/// Compares two test results and fails the test case if they differ.
///
/// \param expected The expected result.
/// \param actual A pointer to the actual result.
template< class Result >
static void
compare_results(const Result& expected, const results::base_result* actual)
{
    std::cout << F("Result is of type '%s'\n") % typeid(*actual).name();

    if (typeid(*actual) == typeid(results::broken) &&
        typeid(expected) != typeid(results::broken)) {
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


/// Wrapper around format_test to define a test case.
///
/// \param name The name of the test case; "__format" will be appended.
/// \param expected The expected formatted string.
/// \param result The result to format.
#define FORMAT_TEST(name, expected, result) \
    ATF_TEST_CASE_WITHOUT_HEAD(name ## __format); \
    ATF_TEST_CASE_BODY(name ## __format) \
    { \
        ATF_REQUIRE_EQ(expected, result.format()); \
    }


/// Creates a test case to validate the good() method.
///
/// \param name The name of the test case; "__good" will be appended.
/// \param expected The expected result of good().
/// \param result The result to check.
#define GOOD_TEST(name, expected, result) \
    ATF_TEST_CASE_WITHOUT_HEAD(name ## __good); \
    ATF_TEST_CASE_BODY(name ## __good) \
    { \
        ATF_REQUIRE_EQ(expected, result.good()); \
    }


/// Performs a test for results::parse() that should succeed.
///
/// \param expected The expected result.
/// \param text The literal input to parse; can include multiple lines.
template< class Result >
static void
parse_ok_test(const Result& expected, const char* text)
{
    std::istringstream input(text);
    results::result_ptr actual = results::parse(input);
    compare_results(expected, actual.get());
}


/// Wrapper around parse_ok_test to define a test case.
///
/// \param name The name of the test case; will be prefixed with "parse__".
/// \param expected The expected result.
/// \param input The literal input to parse.
#define PARSE_OK(name, expected, input) \
    ATF_TEST_CASE_WITHOUT_HEAD(parse__ ## name); \
    ATF_TEST_CASE_BODY(parse__ ## name) \
    { \
        parse_ok_test(expected, input); \
    }


/// Validates a broken test case and fails the test case if invalid.
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


/// Performs a test for results::parse() that should fail.
///
/// \param reason_regexp The reason to match against the broken reason.
/// \param text The literal input to parse; can include multiple lines.
static void
parse_broken_test(const char* reason_regexp, const char* text)
{
    std::istringstream input(text);
    results::result_ptr result = results::parse(input);
    validate_broken(reason_regexp, result.get());
}


/// Wrapper around parse_broken_test to define a test case.
///
/// \param name The name of the test case; will be prefixed with "parse__".
/// \param reason_regexp The reason to match against the broken reason.
/// \param input The literal input to parse.
#define PARSE_BROKEN(name, reason_regexp, input) \
    ATF_TEST_CASE_WITHOUT_HEAD(parse__ ## name); \
    ATF_TEST_CASE_BODY(parse__ ## name) \
    { \
        parse_broken_test(reason_regexp, input); \
    }


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(make_result);
ATF_TEST_CASE_BODY(make_result)
{
    results::result_ptr result = results::make_result(
        results::failed("The message"));
    ATF_REQUIRE(typeid(results::failed) == typeid(*result));
    const results::failed* failed = dynamic_cast< const results::failed* >(
        result.get());
    ATF_REQUIRE(results::failed("The message") == *failed);
}


FORMAT_TEST(broken, "broken: The reason",
            results::broken("The reason"));
FORMAT_TEST(expected_death, "expected_death: The reason",
            results::expected_death("The reason"));
FORMAT_TEST(expected_exit__any, "expected_exit: The reason",
            results::expected_exit(none, "The reason"));
FORMAT_TEST(expected_exit__specific, "expected_exit(3): The reason",
            results::expected_exit(optional< int >(3), "The reason"));
FORMAT_TEST(expected_failure, "expected_failure: The reason",
            results::expected_failure("The reason"));
FORMAT_TEST(expected_signal__any, "expected_signal: The reason",
            results::expected_signal(none, "The reason"));
FORMAT_TEST(expected_signal__specific, "expected_signal(3): The reason",
            results::expected_signal(optional< int >(3), "The reason"));
FORMAT_TEST(expected_timeout, "expected_timeout: The reason",
            results::expected_timeout("The reason"));
FORMAT_TEST(failed, "failed: The reason",
            results::failed("The reason"));
FORMAT_TEST(passed, "passed",
            results::passed());
FORMAT_TEST(skipped, "skipped: The reason",
            results::skipped("The reason"));


GOOD_TEST(broken, false,
          results::broken("The reason"));
GOOD_TEST(expected_death, true,
          results::expected_death("The reason"));
GOOD_TEST(expected_exit__any, true,
          results::expected_exit(none, "The reason"));
GOOD_TEST(expected_exit__specific, true,
          results::expected_exit(optional< int >(3), "The reason"));
GOOD_TEST(expected_failure, true,
          results::expected_failure("The reason"));
GOOD_TEST(expected_signal__any, true,
          results::expected_signal(none, "The reason"));
GOOD_TEST(expected_signal__specific, true,
          results::expected_signal(optional< int >(3), "The reason"));
GOOD_TEST(expected_timeout, true,
          results::expected_timeout("The reason"));
GOOD_TEST(failed, false,
          results::failed("The reason"));
GOOD_TEST(passed, true,
          results::passed());
GOOD_TEST(skipped, true,
          results::skipped("The reason"));


PARSE_BROKEN(empty,
             "Empty.*no new line",
             "");
PARSE_BROKEN(no_newline__unknown,
             "Empty.*no new line",
             "foo");
PARSE_BROKEN(no_newline__known,
             "Empty.*no new line",
             "passed");
PARSE_BROKEN(multiline__no_newline,
             "multiple lines.*foo<<NEWLINE>>bar",
             "failed: foo\nbar");
PARSE_BROKEN(multiline__with_newline,
             "multiple lines.*foo<<NEWLINE>>bar",
             "failed: foo\nbar\n");
PARSE_BROKEN(unknown_status__no_reason,
             "Unknown.*result.*'cba'",
             "cba\n");
PARSE_BROKEN(unknown_status__with_reason,
             "Unknown.*result.*'hgf'",
             "hgf: foo\n");
PARSE_BROKEN(missing_reason__no_delim,
             "failed.*followed by.*reason",
             "failed\n");
PARSE_BROKEN(missing_reason__bad_delim,
             "failed.*followed by.*reason",
             "failed:\n");
PARSE_BROKEN(missing_reason__empty,
             "failed.*followed by.*reason",
             "failed: \n");


PARSE_OK(broken__ok,
         results::broken("a b c"),
         "broken: a b c\n");
PARSE_OK(broken__blanks,
         results::broken("   "),
         "broken:    \n");


PARSE_OK(expected_death__ok,
         results::expected_death("a b c"),
         "expected_death: a b c\n");
PARSE_OK(expected_death__blanks,
         results::expected_death("   "),
         "expected_death:    \n");


PARSE_OK(expected_exit__ok__any,
         results::expected_exit(none, "any exit code"),
         "expected_exit: any exit code\n");
PARSE_OK(expected_exit__ok__specific,
         results::expected_exit(optional< int >(712), "some known exit code"),
         "expected_exit(712): some known exit code\n");
PARSE_BROKEN(expected_exit__bad_int,
             "Invalid integer.*45a3",
             "expected_exit(45a3): this is broken\n");


PARSE_OK(expected_failure__ok,
         results::expected_failure("a b c"),
         "expected_failure: a b c\n");
PARSE_OK(expected_failure__blanks,
         results::expected_failure("   "),
         "expected_failure:    \n");


PARSE_OK(expected_signal__ok__any,
         results::expected_signal(none, "any signal code"),
         "expected_signal: any signal code\n");
PARSE_OK(expected_signal__ok__specific,
         results::expected_signal(optional< int >(712), "some known signal code"),
         "expected_signal(712): some known signal code\n");
PARSE_BROKEN(expected_signal__bad_int,
             "Invalid integer.*45a3",
             "expected_signal(45a3): this is broken\n");


PARSE_OK(expected_timeout__ok,
         results::expected_timeout("a b c"),
         "expected_timeout: a b c\n");
PARSE_OK(expected_timeout__blanks,
         results::expected_timeout("   "),
         "expected_timeout:    \n");


PARSE_OK(failed__ok,
         results::failed("a b c"),
         "failed: a b c\n");
PARSE_OK(failed__blanks,
         results::failed("   "),
         "failed:    \n");


PARSE_OK(passed__ok,
         results::passed(),
         "passed\n");
PARSE_BROKEN(passed__reason,
             "cannot have a reason",
             "passed a b c\n");


PARSE_OK(skipped__ok,
         results::skipped("a b c"),
         "skipped: a b c\n");
PARSE_OK(skipped__blanks,
         results::skipped("   "),
         "skipped:    \n");


ATF_TEST_CASE_WITHOUT_HEAD(load__ok);
ATF_TEST_CASE_BODY(load__ok)
{
    std::ofstream output("result.txt");
    ATF_REQUIRE(output);
    output << "skipped: a b c\n";
    output.close();

    results::result_ptr result = results::load(utils::fs::path("result.txt"));
    try {
        const results::skipped* skipped =
            dynamic_cast< const results::skipped* >(result.get());
        ATF_REQUIRE_EQ("a b c", skipped->reason);
    } catch (const std::bad_cast* e) {
        fail("Invalid result type returned");
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(load__missing_file);
ATF_TEST_CASE_BODY(load__missing_file)
{
    results::result_ptr result = results::load(utils::fs::path("result.txt"));
    ATF_REQUIRE(NULL == result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(load__format_error);
ATF_TEST_CASE_BODY(load__format_error)
{
    std::ofstream output("abc.txt");
    ATF_REQUIRE(output);
    output << "passed: foo\n";
    output.close();

    results::result_ptr result = results::load(utils::fs::path("abc.txt"));
    const results::broken* broken = dynamic_cast< const results::broken* >(
        result.get());
    ATF_REQUIRE_MATCH("cannot have a reason", broken->reason);
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust_with_status__missing);
ATF_TEST_CASE_BODY(adjust_with_status__missing)
{
    const process::status status = process::status::fake_exited(EXIT_SUCCESS);
    validate_broken("Premature exit: exited with code 0",
                    results::adjust_with_status(results::result_ptr(NULL),
                                                status).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust_with_status__broken);
ATF_TEST_CASE_BODY(adjust_with_status__broken)
{
    const results::broken broken("Passthrough");
    const process::status status = process::status::fake_exited(EXIT_SUCCESS);
    validate_broken("Passthrough",
                    results::adjust_with_status(results::make_result(broken),
                                                status).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust_with_status__expected_death__ok);
ATF_TEST_CASE_BODY(adjust_with_status__expected_death__ok)
{
    const results::expected_death death("The reason");
    const process::status status = process::status::fake_signaled(SIGINT, true);
    compare_results(death,
                    results::adjust_with_status(results::make_result(death),
                                                status).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust_with_status__expected_exit__ok);
ATF_TEST_CASE_BODY(adjust_with_status__expected_exit__ok)
{
    const process::status success = process::status::fake_exited(EXIT_SUCCESS);
    const process::status failure = process::status::fake_exited(EXIT_FAILURE);

    const results::expected_exit any_code(none, "The reason");
    compare_results(any_code,
                    results::adjust_with_status(results::make_result(any_code),
                                                success).get());
    compare_results(any_code,
                    results::adjust_with_status(results::make_result(any_code),
                                                failure).get());

    const results::expected_exit a_code(utils::make_optional(EXIT_FAILURE),
                                        "The reason");
    compare_results(a_code,
                    results::adjust_with_status(results::make_result(a_code),
                                                failure).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust_with_status__expected_exit__broken);
ATF_TEST_CASE_BODY(adjust_with_status__expected_exit__broken)
{
    const process::status sig3 = process::status::fake_signaled(3, false);
    const process::status success = process::status::fake_exited(EXIT_SUCCESS);

    const results::expected_exit any_code(none, "The reason");
    validate_broken("Expected clean exit but received signal 3",
                    results::adjust_with_status(results::make_result(any_code),
                                                sig3).get());

    const results::expected_exit a_code(utils::make_optional(EXIT_FAILURE),
                                        "The reason");
    validate_broken("Expected clean exit with code 1 but got code 0",
                    results::adjust_with_status(results::make_result(a_code),
                                                success).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust_with_status__expected_failure__ok);
ATF_TEST_CASE_BODY(adjust_with_status__expected_failure__ok)
{
    const results::expected_failure failure("The reason");
    const process::status status = process::status::fake_exited(EXIT_SUCCESS);
    compare_results(failure,
                    results::adjust_with_status(results::make_result(failure),
                                                status).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust_with_status__expected_failure__broken);
ATF_TEST_CASE_BODY(adjust_with_status__expected_failure__broken)
{
    const process::status success = process::status::fake_exited(EXIT_SUCCESS);
    const process::status failure = process::status::fake_exited(EXIT_FAILURE);
    const process::status sig3 = process::status::fake_signaled(3, true);

    const results::expected_failure xfailure("The reason");
    validate_broken("Expected failure should have reported success but "
                    "exited with code 1",
                    results::adjust_with_status(results::make_result(xfailure),
                                                failure).get());
    validate_broken("Expected failure should have reported success but "
                    "received signal 3",
                    results::adjust_with_status(results::make_result(xfailure),
                                                sig3).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust_with_status__expected_signal__ok);
ATF_TEST_CASE_BODY(adjust_with_status__expected_signal__ok)
{
    const process::status sig1 = process::status::fake_signaled(1, false);
    const process::status sig3 = process::status::fake_signaled(3, true);

    const results::expected_signal any_sig(none, "The reason");
    compare_results(any_sig,
                    results::adjust_with_status(results::make_result(any_sig),
                                                sig1).get());
    compare_results(any_sig,
                    results::adjust_with_status(results::make_result(any_sig),
                                                sig3).get());

    const results::expected_signal a_sig(utils::make_optional(3),
                                         "The reason");
    compare_results(a_sig,
                    results::adjust_with_status(results::make_result(a_sig),
                                                sig3).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust_with_status__expected_signal__broken);
ATF_TEST_CASE_BODY(adjust_with_status__expected_signal__broken)
{
    const process::status sig5 = process::status::fake_signaled(5, false);
    const process::status success = process::status::fake_exited(EXIT_SUCCESS);

    const results::expected_signal any_sig(none, "The reason");
    validate_broken("Expected signal but exited with code 0",
                    results::adjust_with_status(results::make_result(any_sig),
                                                success).get());

    const results::expected_signal a_sig(utils::make_optional(4),
                                         "The reason");
    validate_broken("Expected signal 4 but got 5",
                    results::adjust_with_status(results::make_result(a_sig),
                                                sig5).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust_with_status__expected_timeout__broken);
ATF_TEST_CASE_BODY(adjust_with_status__expected_timeout__broken)
{
    const results::expected_timeout timeout("The reason");
    const process::status status = process::status::fake_exited(EXIT_SUCCESS);
    validate_broken("Expected timeout but exited with code 0",
                    results::adjust_with_status(results::make_result(timeout),
                                                status).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust_with_status__failed__ok);
ATF_TEST_CASE_BODY(adjust_with_status__failed__ok)
{
    const results::failed failed("The reason");
    const process::status status = process::status::fake_exited(EXIT_FAILURE);
    compare_results(failed,
                    results::adjust_with_status(results::make_result(failed),
                                                status).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust_with_status__failed__broken);
ATF_TEST_CASE_BODY(adjust_with_status__failed__broken)
{
    const process::status success = process::status::fake_exited(EXIT_SUCCESS);
    const process::status failure = process::status::fake_exited(EXIT_FAILURE);
    const process::status sig3 = process::status::fake_signaled(3, true);

    const results::failed failed("The reason");
    validate_broken("Failed test case should have reported failure but "
                    "exited with code 0",
                    results::adjust_with_status(results::make_result(failed),
                                                success).get());
    validate_broken("Failed test case should have reported failure but "
                    "received signal 3",
                    results::adjust_with_status(results::make_result(failed),
                                                sig3).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust_with_status__passed__ok);
ATF_TEST_CASE_BODY(adjust_with_status__passed__ok)
{
    const results::passed passed;
    const process::status status = process::status::fake_exited(EXIT_SUCCESS);
    compare_results(passed,
                    results::adjust_with_status(results::make_result(passed),
                                                status).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust_with_status__passed__broken);
ATF_TEST_CASE_BODY(adjust_with_status__passed__broken)
{
    const process::status success = process::status::fake_exited(EXIT_SUCCESS);
    const process::status failure = process::status::fake_exited(EXIT_FAILURE);
    const process::status sig3 = process::status::fake_signaled(3, true);

    const results::passed passed;
    validate_broken("Passed test case should have reported success but "
                    "exited with code 1",
                    results::adjust_with_status(results::make_result(passed),
                                                failure).get());
    validate_broken("Passed test case should have reported success but "
                    "received signal 3",
                    results::adjust_with_status(results::make_result(passed),
                                                sig3).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust_with_status__skipped__ok);
ATF_TEST_CASE_BODY(adjust_with_status__skipped__ok)
{
    const results::skipped skipped("The reason");
    const process::status status = process::status::fake_exited(EXIT_SUCCESS);
    compare_results(skipped,
                    results::adjust_with_status(results::make_result(skipped),
                                                status).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust_with_status__skipped__broken);
ATF_TEST_CASE_BODY(adjust_with_status__skipped__broken)
{
    const process::status success = process::status::fake_exited(EXIT_SUCCESS);
    const process::status failure = process::status::fake_exited(EXIT_FAILURE);
    const process::status sig3 = process::status::fake_signaled(3, true);

    const results::skipped skipped("The reason");
    validate_broken("Skipped test case should have reported success but "
                    "exited with code 1",
                    results::adjust_with_status(results::make_result(skipped),
                                                failure).get());
    validate_broken("Skipped test case should have reported success but "
                    "received signal 3",
                    results::adjust_with_status(results::make_result(skipped),
                                                sig3).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust_with_timeout__expected_timeout);
ATF_TEST_CASE_BODY(adjust_with_timeout__expected_timeout)
{
    const results::expected_timeout timeout("The reason");
    compare_results(timeout, results::adjust_with_timeout(
        results::make_result(timeout), datetime::delta()).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust_with_timeout__timed_out);
ATF_TEST_CASE_BODY(adjust_with_timeout__timed_out)
{
    const results::broken broken("Ignore this");
    validate_broken("Test case timed out after 123 seconds",
        results::adjust_with_timeout(results::make_result(broken),
                                     datetime::delta(123, 0)).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust__body_ok__no_cleanup);
ATF_TEST_CASE_BODY(adjust__body_ok__no_cleanup)
{
    const engine::atf_test_program test_program(fs::path("non-existent"),
                                                fs::path("."), "unused-suite");

    engine::properties_map metadata;
    const engine::atf_test_case test_case = engine::atf_test_case::from_properties(
        test_program, "name", metadata);
    const results::passed result;
    compare_results(result, results::adjust(test_case,
        utils::make_optional(process::status::fake_exited(EXIT_SUCCESS)),
        none,
        results::make_result(result)).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust__body_ok__cleanup_ok);
ATF_TEST_CASE_BODY(adjust__body_ok__cleanup_ok)
{
    const engine::atf_test_program test_program(fs::path("non-existent"),
                                                fs::path("."), "unused-suite");

    engine::properties_map metadata;
    metadata["has.cleanup"] = "true";
    const engine::atf_test_case test_case = engine::atf_test_case::from_properties(
        test_program, "name", metadata);
    const results::passed result;
    compare_results(result, results::adjust(test_case,
        utils::make_optional(process::status::fake_exited(EXIT_SUCCESS)),
        utils::make_optional(process::status::fake_exited(EXIT_SUCCESS)),
        results::make_result(result)).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust__body_ok__cleanup_bad);
ATF_TEST_CASE_BODY(adjust__body_ok__cleanup_bad)
{
    const engine::atf_test_program test_program(fs::path("non-existent"),
                                                fs::path("."), "unused-suite");

    engine::properties_map metadata;
    metadata["has.cleanup"] = "true";
    const engine::atf_test_case test_case = engine::atf_test_case::from_properties(
        test_program, "name", metadata);
    const results::passed result;
    validate_broken("cleanup.*not.*successful", results::adjust(test_case,
        utils::make_optional(process::status::fake_exited(EXIT_SUCCESS)),
        utils::make_optional(process::status::fake_exited(EXIT_FAILURE)),
        results::make_result(result)).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust__body_ok__cleanup_timeout);
ATF_TEST_CASE_BODY(adjust__body_ok__cleanup_timeout)
{
    const engine::atf_test_program test_program(fs::path("non-existent"),
                                                fs::path("."), "unused-suite");

    engine::properties_map metadata;
    metadata["has.cleanup"] = "true";
    metadata["timeout"] = "123";
    const engine::atf_test_case test_case = engine::atf_test_case::from_properties(
        test_program, "name", metadata);
    const results::passed result;
    validate_broken("cleanup.*timed out.*123", results::adjust(test_case,
        utils::make_optional(process::status::fake_exited(EXIT_SUCCESS)),
        none,
        results::make_result(result)).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust__body_bad__cleanup_ok);
ATF_TEST_CASE_BODY(adjust__body_bad__cleanup_ok)
{
    const engine::atf_test_program test_program(fs::path("non-existent"),
                                                fs::path("."), "unused-suite");

    engine::properties_map metadata;
    metadata["has.cleanup"] = "true";
    const engine::atf_test_case test_case = engine::atf_test_case::from_properties(
        test_program, "name", metadata);
    const results::failed result("The reason");
    compare_results(result, results::adjust(test_case,
        utils::make_optional(process::status::fake_exited(EXIT_FAILURE)),
        utils::make_optional(process::status::fake_exited(EXIT_SUCCESS)),
        results::make_result(result)).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust__body_bad__cleanup_bad);
ATF_TEST_CASE_BODY(adjust__body_bad__cleanup_bad)
{
    const engine::atf_test_program test_program(fs::path("non-existent"),
                                                fs::path("."), "unused-suite");

    engine::properties_map metadata;
    metadata["has.cleanup"] = "true";
    const engine::atf_test_case test_case = engine::atf_test_case::from_properties(
        test_program, "name", metadata);
    const results::failed result("The reason");
    compare_results(result, results::adjust(test_case,
        utils::make_optional(process::status::fake_exited(EXIT_FAILURE)),
        utils::make_optional(process::status::fake_exited(EXIT_FAILURE)),
        results::make_result(result)).get());
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__expected_death);
ATF_TEST_CASE_BODY(integration__expected_death)
{
    run_test_case(this, "expected_death", fs::path("result.txt"));
    results::result_ptr result = results::load(fs::path("result.txt"));
    compare_results(results::expected_death("This supposedly dies"),
                    result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__expected_exit__any);
ATF_TEST_CASE_BODY(integration__expected_exit__any)
{
    run_test_case(this, "expected_exit__any", fs::path("result.txt"));
    results::result_ptr result = results::load(fs::path("result.txt"));
    compare_results(results::expected_exit(none, "This supposedly exits with "
                                           "any code"),
                    result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__expected_exit__specific);
ATF_TEST_CASE_BODY(integration__expected_exit__specific)
{
    run_test_case(this, "expected_exit__specific", fs::path("result.txt"));
    results::result_ptr result = results::load(fs::path("result.txt"));
    compare_results(results::expected_exit(optional< int >(312), "This "
                                           "supposedly exits"),
                    result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__expected_failure);
ATF_TEST_CASE_BODY(integration__expected_failure)
{
    run_test_case(this, "expected_failure", fs::path("result.txt"));
    results::result_ptr result = results::load(fs::path("result.txt"));
    compare_results(results::expected_failure("This supposedly fails as "
                                              "expected: The failure"),
                    result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__expected_signal__any);
ATF_TEST_CASE_BODY(integration__expected_signal__any)
{
    run_test_case(this, "expected_signal__any", fs::path("result.txt"));
    results::result_ptr result = results::load(fs::path("result.txt"));
    compare_results(results::expected_signal(none, "This supposedly gets any "
                                             "signal"), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__expected_signal__specific);
ATF_TEST_CASE_BODY(integration__expected_signal__specific)
{
    run_test_case(this, "expected_signal__specific", fs::path("result.txt"));
    results::result_ptr result = results::load(fs::path("result.txt"));
    compare_results(results::expected_signal(optional<int >(756), "This "
                                             "supposedly gets a signal"),
                    result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__expected_timeout);
ATF_TEST_CASE_BODY(integration__expected_timeout)
{
    run_test_case(this, "expected_timeout", fs::path("result.txt"));
    results::result_ptr result = results::load(fs::path("result.txt"));
    compare_results(results::expected_timeout("This supposedly times out"),
                    result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__failed);
ATF_TEST_CASE_BODY(integration__failed)
{
    run_test_case(this, "failed", fs::path("result.txt"));
    results::result_ptr result = results::load(fs::path("result.txt"));
    compare_results(results::failed("Failed on purpose"),
                    result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__multiline);
ATF_TEST_CASE_BODY(integration__multiline)
{
    run_test_case(this, "multiline", fs::path("result.txt"));
    results::result_ptr result = results::load(fs::path("result.txt"));
    validate_broken("multiple lines.*skipped: word line1<<NEWLINE>>line2",
                    result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__passed);
ATF_TEST_CASE_BODY(integration__passed)
{
    run_test_case(this, "passed", fs::path("result.txt"));
    results::result_ptr result = results::load(fs::path("result.txt"));
    compare_results(results::passed(), result.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__skipped);
ATF_TEST_CASE_BODY(integration__skipped)
{
    run_test_case(this, "skipped", fs::path("result.txt"));
    results::result_ptr result = results::load(fs::path("result.txt"));
    compare_results(results::skipped("Skipped on purpose"),
                    result.get());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, make_result);

    ATF_ADD_TEST_CASE(tcs, broken__format);
    ATF_ADD_TEST_CASE(tcs, broken__good);
    ATF_ADD_TEST_CASE(tcs, expected_death__format);
    ATF_ADD_TEST_CASE(tcs, expected_death__good);
    ATF_ADD_TEST_CASE(tcs, expected_exit__any__format);
    ATF_ADD_TEST_CASE(tcs, expected_exit__any__good);
    ATF_ADD_TEST_CASE(tcs, expected_exit__specific__format);
    ATF_ADD_TEST_CASE(tcs, expected_exit__specific__good);
    ATF_ADD_TEST_CASE(tcs, expected_failure__format);
    ATF_ADD_TEST_CASE(tcs, expected_failure__good);
    ATF_ADD_TEST_CASE(tcs, expected_signal__any__format);
    ATF_ADD_TEST_CASE(tcs, expected_signal__any__good);
    ATF_ADD_TEST_CASE(tcs, expected_signal__specific__format);
    ATF_ADD_TEST_CASE(tcs, expected_signal__specific__good);
    ATF_ADD_TEST_CASE(tcs, expected_timeout__format);
    ATF_ADD_TEST_CASE(tcs, expected_timeout__good);
    ATF_ADD_TEST_CASE(tcs, failed__format);
    ATF_ADD_TEST_CASE(tcs, failed__good);
    ATF_ADD_TEST_CASE(tcs, passed__format);
    ATF_ADD_TEST_CASE(tcs, passed__good);
    ATF_ADD_TEST_CASE(tcs, skipped__format);
    ATF_ADD_TEST_CASE(tcs, skipped__good);

    ATF_ADD_TEST_CASE(tcs, parse__empty);
    ATF_ADD_TEST_CASE(tcs, parse__no_newline__unknown);
    ATF_ADD_TEST_CASE(tcs, parse__no_newline__known);
    ATF_ADD_TEST_CASE(tcs, parse__multiline__no_newline);
    ATF_ADD_TEST_CASE(tcs, parse__multiline__with_newline);
    ATF_ADD_TEST_CASE(tcs, parse__unknown_status__no_reason);
    ATF_ADD_TEST_CASE(tcs, parse__unknown_status__with_reason);
    ATF_ADD_TEST_CASE(tcs, parse__missing_reason__no_delim);
    ATF_ADD_TEST_CASE(tcs, parse__missing_reason__bad_delim);
    ATF_ADD_TEST_CASE(tcs, parse__missing_reason__empty);

    ATF_ADD_TEST_CASE(tcs, parse__broken__ok);
    ATF_ADD_TEST_CASE(tcs, parse__broken__blanks);

    ATF_ADD_TEST_CASE(tcs, parse__expected_death__ok);
    ATF_ADD_TEST_CASE(tcs, parse__expected_death__blanks);

    ATF_ADD_TEST_CASE(tcs, parse__expected_exit__ok__any);
    ATF_ADD_TEST_CASE(tcs, parse__expected_exit__ok__specific);
    ATF_ADD_TEST_CASE(tcs, parse__expected_exit__bad_int);

    ATF_ADD_TEST_CASE(tcs, parse__expected_failure__ok);
    ATF_ADD_TEST_CASE(tcs, parse__expected_failure__blanks);

    ATF_ADD_TEST_CASE(tcs, parse__expected_signal__ok__any);
    ATF_ADD_TEST_CASE(tcs, parse__expected_signal__ok__specific);
    ATF_ADD_TEST_CASE(tcs, parse__expected_signal__bad_int);

    ATF_ADD_TEST_CASE(tcs, parse__expected_timeout__ok);
    ATF_ADD_TEST_CASE(tcs, parse__expected_timeout__blanks);

    ATF_ADD_TEST_CASE(tcs, parse__failed__ok);
    ATF_ADD_TEST_CASE(tcs, parse__failed__blanks);

    ATF_ADD_TEST_CASE(tcs, parse__passed__ok);
    ATF_ADD_TEST_CASE(tcs, parse__passed__reason);

    ATF_ADD_TEST_CASE(tcs, parse__skipped__ok);
    ATF_ADD_TEST_CASE(tcs, parse__skipped__blanks);

    ATF_ADD_TEST_CASE(tcs, load__ok);
    ATF_ADD_TEST_CASE(tcs, load__missing_file);
    ATF_ADD_TEST_CASE(tcs, load__format_error);

    ATF_ADD_TEST_CASE(tcs, adjust_with_status__missing);
    ATF_ADD_TEST_CASE(tcs, adjust_with_status__broken);
    ATF_ADD_TEST_CASE(tcs, adjust_with_status__expected_death__ok);
    ATF_ADD_TEST_CASE(tcs, adjust_with_status__expected_exit__ok);
    ATF_ADD_TEST_CASE(tcs, adjust_with_status__expected_exit__broken);
    ATF_ADD_TEST_CASE(tcs, adjust_with_status__expected_failure__ok);
    ATF_ADD_TEST_CASE(tcs, adjust_with_status__expected_failure__broken);
    ATF_ADD_TEST_CASE(tcs, adjust_with_status__expected_signal__ok);
    ATF_ADD_TEST_CASE(tcs, adjust_with_status__expected_signal__broken);
    ATF_ADD_TEST_CASE(tcs, adjust_with_status__expected_timeout__broken);
    ATF_ADD_TEST_CASE(tcs, adjust_with_status__failed__ok);
    ATF_ADD_TEST_CASE(tcs, adjust_with_status__failed__broken);
    ATF_ADD_TEST_CASE(tcs, adjust_with_status__passed__ok);
    ATF_ADD_TEST_CASE(tcs, adjust_with_status__passed__broken);
    ATF_ADD_TEST_CASE(tcs, adjust_with_status__skipped__ok);
    ATF_ADD_TEST_CASE(tcs, adjust_with_status__skipped__broken);

    ATF_ADD_TEST_CASE(tcs, adjust_with_timeout__expected_timeout);
    ATF_ADD_TEST_CASE(tcs, adjust_with_timeout__timed_out);

    ATF_ADD_TEST_CASE(tcs, adjust__body_ok__no_cleanup);
    ATF_ADD_TEST_CASE(tcs, adjust__body_ok__cleanup_ok);
    ATF_ADD_TEST_CASE(tcs, adjust__body_ok__cleanup_bad);
    ATF_ADD_TEST_CASE(tcs, adjust__body_ok__cleanup_timeout);
    ATF_ADD_TEST_CASE(tcs, adjust__body_bad__cleanup_ok);
    ATF_ADD_TEST_CASE(tcs, adjust__body_bad__cleanup_bad);

    ATF_ADD_TEST_CASE(tcs, integration__expected_death);
    ATF_ADD_TEST_CASE(tcs, integration__expected_exit__any);
    ATF_ADD_TEST_CASE(tcs, integration__expected_exit__specific);
    ATF_ADD_TEST_CASE(tcs, integration__expected_failure);
    ATF_ADD_TEST_CASE(tcs, integration__expected_signal__any);
    ATF_ADD_TEST_CASE(tcs, integration__expected_signal__specific);
    ATF_ADD_TEST_CASE(tcs, integration__expected_timeout);
    ATF_ADD_TEST_CASE(tcs, integration__failed);
    ATF_ADD_TEST_CASE(tcs, integration__multiline);
    ATF_ADD_TEST_CASE(tcs, integration__passed);
    ATF_ADD_TEST_CASE(tcs, integration__skipped);
}
