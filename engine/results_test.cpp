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

#include <typeinfo>

#include <atf-c++.hpp>

#include "engine/results.ipp"

namespace results = engine::results;

using utils::none;
using utils::optional;


namespace {


template< typename Result >
static void
unique_address_test(void)
{
    const Result result1;
    {
        const Result result2 = result1;
        const Result result3;
        ATF_REQUIRE(result1.unique_address() == result2.unique_address());
        ATF_REQUIRE(result1.unique_address() != result3.unique_address());
        ATF_REQUIRE(result2.unique_address() != result3.unique_address());
    }
    ATF_REQUIRE(result1.unique_address() == result1.unique_address());
}


template< typename Result >
static void
unique_address_test(const std::string& reason)
{
    const Result result1(reason);
    {
        const Result result2 = result1;
        const Result result3(reason);
        ATF_REQUIRE(result1.unique_address() == result2.unique_address());
        ATF_REQUIRE(result1.unique_address() != result3.unique_address());
        ATF_REQUIRE(result2.unique_address() != result3.unique_address());
    }
    ATF_REQUIRE(result1.unique_address() == result1.unique_address());
}


/// Wrapper around unique_address_test to define a test case.
///
/// \param name The name of the test result; "__unique_address" will be
///     appended, and results::<name> must exist.
#define UNIQUE_ADDRESS_TEST(name) \
    ATF_TEST_CASE_WITHOUT_HEAD(name ## __unique_address); \
    ATF_TEST_CASE_BODY(name ## __unique_address) \
    { \
        unique_address_test< results::name >(); \
    }


/// Wrapper around unique_address_test to define a test case.
///
/// \param name The name of the test result; "__unique_address" will be
///     appended, and results::<name> must exist.
/// \param reason An arbitrary reason text for the test.
#define UNIQUE_ADDRESS_WITH_REASON_TEST(name, reason) \
    ATF_TEST_CASE_WITHOUT_HEAD(name ## __unique_address); \
    ATF_TEST_CASE_BODY(name ## __unique_address) \
    { \
        unique_address_test< results::name >(reason); \
    }


/// Creates a test case to validate the format() method.
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


UNIQUE_ADDRESS_WITH_REASON_TEST(broken, "The reason");
UNIQUE_ADDRESS_WITH_REASON_TEST(expected_failure, "The reason");
UNIQUE_ADDRESS_WITH_REASON_TEST(failed, "The reason");
UNIQUE_ADDRESS_TEST(passed);
UNIQUE_ADDRESS_WITH_REASON_TEST(skipped, "The reason");


FORMAT_TEST(broken, "broken: The reason", results::broken("The reason"));
FORMAT_TEST(expected_failure, "expected_failure: The reason",
            results::expected_failure("The reason"));
FORMAT_TEST(failed, "failed: The reason", results::failed("The reason"));
FORMAT_TEST(passed, "passed", results::passed());
FORMAT_TEST(skipped, "skipped: The reason", results::skipped("The reason"));


GOOD_TEST(broken, false, results::broken("The reason"));
GOOD_TEST(expected_failure, true, results::expected_failure("The reason"));
GOOD_TEST(failed, false, results::failed("The reason"));
GOOD_TEST(passed, true, results::passed());
GOOD_TEST(skipped, true, results::skipped("The reason"));


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, make_result);

    ATF_ADD_TEST_CASE(tcs, broken__unique_address);
    ATF_ADD_TEST_CASE(tcs, broken__format);
    ATF_ADD_TEST_CASE(tcs, broken__good);
    ATF_ADD_TEST_CASE(tcs, expected_failure__unique_address);
    ATF_ADD_TEST_CASE(tcs, expected_failure__format);
    ATF_ADD_TEST_CASE(tcs, expected_failure__good);
    ATF_ADD_TEST_CASE(tcs, failed__unique_address);
    ATF_ADD_TEST_CASE(tcs, failed__format);
    ATF_ADD_TEST_CASE(tcs, failed__good);
    ATF_ADD_TEST_CASE(tcs, passed__unique_address);
    ATF_ADD_TEST_CASE(tcs, passed__format);
    ATF_ADD_TEST_CASE(tcs, passed__good);
    ATF_ADD_TEST_CASE(tcs, skipped__unique_address);
    ATF_ADD_TEST_CASE(tcs, skipped__format);
    ATF_ADD_TEST_CASE(tcs, skipped__good);
}
