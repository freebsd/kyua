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

#include <atf-c++.hpp>

#include "engine/test_result.hpp"

using engine::test_result;


namespace {


/// Creates a test case to validate the getters.
///
/// \param name The name of the test case; "__getters" will be appended.
/// \param expected_type The expected type of the result.
/// \param expected_reason The expected reason for the result.
/// \param result The result to query.
#define GETTERS_TEST(name, expected_type, expected_reason, result) \
    ATF_TEST_CASE_WITHOUT_HEAD(name ## __getters); \
    ATF_TEST_CASE_BODY(name ## __getters) \
    { \
        ATF_REQUIRE(expected_type == result.type()); \
        ATF_REQUIRE_EQ(expected_reason, result.reason());  \
    }


/// Creates a test case to validate the good() method.
///
/// \param name The name of the test case; "__good" will be appended.
/// \param expected The expected result of good().
/// \param result_type The result type to check.
#define GOOD_TEST(name, expected, result_type) \
    ATF_TEST_CASE_WITHOUT_HEAD(name ## __good); \
    ATF_TEST_CASE_BODY(name ## __good) \
    { \
        ATF_REQUIRE_EQ(expected, test_result(result_type).good()); \
    }


}  // anonymous namespace


GETTERS_TEST(broken, test_result::broken, "The reason",
             test_result(test_result::broken, "The reason"));
GETTERS_TEST(expected_failure, test_result::expected_failure, "The reason",
             test_result(test_result::expected_failure, "The reason"));
GETTERS_TEST(failed, test_result::failed, "The reason",
             test_result(test_result::failed, "The reason"));
GETTERS_TEST(passed, test_result::passed, "",
             test_result(test_result::passed));
GETTERS_TEST(skipped, test_result::skipped, "The reason",
             test_result(test_result::skipped, "The reason"));


GOOD_TEST(broken, false, test_result::broken);
GOOD_TEST(expected_failure, true, test_result::expected_failure);
GOOD_TEST(failed, false, test_result::failed);
GOOD_TEST(passed, true, test_result::passed);
GOOD_TEST(skipped, true, test_result::skipped);


ATF_TEST_CASE_WITHOUT_HEAD(operator_eq);
ATF_TEST_CASE_BODY(operator_eq)
{
    const test_result result1(test_result::broken, "Foo");
    const test_result result2(test_result::broken, "Foo");
    const test_result result3(test_result::broken, "Bar");
    const test_result result4(test_result::failed, "Foo");

    ATF_REQUIRE(  result1 == result1);
    ATF_REQUIRE(  result1 == result2);
    ATF_REQUIRE(!(result1 == result3));
    ATF_REQUIRE(!(result1 == result4));
}


ATF_TEST_CASE_WITHOUT_HEAD(operator_ne);
ATF_TEST_CASE_BODY(operator_ne)
{
    const test_result result1(test_result::broken, "Foo");
    const test_result result2(test_result::broken, "Foo");
    const test_result result3(test_result::broken, "Bar");
    const test_result result4(test_result::failed, "Foo");

    ATF_REQUIRE(!(result1 != result1));
    ATF_REQUIRE(!(result1 != result2));
    ATF_REQUIRE(  result1 != result3);
    ATF_REQUIRE(  result1 != result4);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, broken__getters);
    ATF_ADD_TEST_CASE(tcs, broken__good);
    ATF_ADD_TEST_CASE(tcs, expected_failure__getters);
    ATF_ADD_TEST_CASE(tcs, expected_failure__good);
    ATF_ADD_TEST_CASE(tcs, failed__getters);
    ATF_ADD_TEST_CASE(tcs, failed__good);
    ATF_ADD_TEST_CASE(tcs, passed__getters);
    ATF_ADD_TEST_CASE(tcs, passed__good);
    ATF_ADD_TEST_CASE(tcs, skipped__getters);
    ATF_ADD_TEST_CASE(tcs, skipped__good);
    ATF_ADD_TEST_CASE(tcs, operator_eq);
    ATF_ADD_TEST_CASE(tcs, operator_ne);
}
