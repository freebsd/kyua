// Copyright 2010 Google Inc.
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

#include <cstdlib>

#include <atf-c++.hpp>


ATF_TEST_CASE_WITHOUT_HEAD(expected_death);
ATF_TEST_CASE_BODY(expected_death)
{
    expect_death("This supposedly dies");
    std::abort();
}


ATF_TEST_CASE_WITHOUT_HEAD(expected_exit__any);
ATF_TEST_CASE_BODY(expected_exit__any)
{
    expect_exit(-1, "This supposedly exits with any code");
    std::abort();
}


ATF_TEST_CASE_WITHOUT_HEAD(expected_exit__specific);
ATF_TEST_CASE_BODY(expected_exit__specific)
{
    expect_exit(312, "This supposedly exits");
    std::abort();
}


ATF_TEST_CASE_WITHOUT_HEAD(expected_failure);
ATF_TEST_CASE_BODY(expected_failure)
{
    expect_fail("This supposedly fails as expected");
    fail("The failure");
}


ATF_TEST_CASE_WITHOUT_HEAD(expected_signal__any);
ATF_TEST_CASE_BODY(expected_signal__any)
{
    expect_signal(-1, "This supposedly gets any signal");
    std::abort();
}


ATF_TEST_CASE_WITHOUT_HEAD(expected_signal__specific);
ATF_TEST_CASE_BODY(expected_signal__specific)
{
    expect_signal(756, "This supposedly gets a signal");
    std::abort();
}


ATF_TEST_CASE_WITHOUT_HEAD(expected_timeout);
ATF_TEST_CASE_BODY(expected_timeout)
{
    expect_timeout("This supposedly times out");
    std::abort();
}


ATF_TEST_CASE_WITHOUT_HEAD(failed);
ATF_TEST_CASE_BODY(failed)
{
    fail("Failed on purpose");
}


ATF_TEST_CASE_WITHOUT_HEAD(multiline);
ATF_TEST_CASE_BODY(multiline)
{
    skip("word line1\nline2");
}


ATF_TEST_CASE_WITHOUT_HEAD(passed);
ATF_TEST_CASE_BODY(passed)
{
}


ATF_TEST_CASE_WITHOUT_HEAD(skipped);
ATF_TEST_CASE_BODY(skipped)
{
    skip("Skipped on purpose");
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, expected_death);
    ATF_ADD_TEST_CASE(tcs, expected_exit__any);
    ATF_ADD_TEST_CASE(tcs, expected_exit__specific);
    ATF_ADD_TEST_CASE(tcs, expected_failure);
    ATF_ADD_TEST_CASE(tcs, expected_signal__any);
    ATF_ADD_TEST_CASE(tcs, expected_signal__specific);
    ATF_ADD_TEST_CASE(tcs, expected_timeout);
    ATF_ADD_TEST_CASE(tcs, failed);
    ATF_ADD_TEST_CASE(tcs, multiline);
    ATF_ADD_TEST_CASE(tcs, passed);
    ATF_ADD_TEST_CASE(tcs, skipped);
}
