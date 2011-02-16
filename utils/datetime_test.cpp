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
#include <time.h>
}

#include <atf-c++.hpp>

#include "utils/datetime.hpp"

namespace datetime = utils::datetime;


ATF_TEST_CASE_WITHOUT_HEAD(delta__defaults);
ATF_TEST_CASE_BODY(delta__defaults)
{
    const datetime::delta delta;
    ATF_REQUIRE_EQ(0, delta.seconds);
    ATF_REQUIRE_EQ(0, delta.useconds);
}


ATF_TEST_CASE_WITHOUT_HEAD(delta__overrides);
ATF_TEST_CASE_BODY(delta__overrides)
{
    const datetime::delta delta(1, 2);
    ATF_REQUIRE_EQ(1, delta.seconds);
    ATF_REQUIRE_EQ(2, delta.useconds);
}


ATF_TEST_CASE_WITHOUT_HEAD(delta__equals);
ATF_TEST_CASE_BODY(delta__equals)
{
    ATF_REQUIRE(datetime::delta() == datetime::delta());
    ATF_REQUIRE(datetime::delta() == datetime::delta(0, 0));
    ATF_REQUIRE(datetime::delta(1, 2) == datetime::delta(1, 2));

    ATF_REQUIRE(!(datetime::delta() == datetime::delta(0, 1)));
    ATF_REQUIRE(!(datetime::delta() == datetime::delta(1, 0)));
    ATF_REQUIRE(!(datetime::delta(1, 2) == datetime::delta(2, 1)));
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__copy);
ATF_TEST_CASE_BODY(timestamp__copy)
{
    const datetime::timestamp ts1 = datetime::timestamp::from_values(
        2011, 2, 16, 19, 15, 30);
    {
        const datetime::timestamp ts2 = ts1;
        const datetime::timestamp ts3 = datetime::timestamp::from_values(
            2012, 2, 16, 19, 15, 30);
        ATF_REQUIRE_EQ("2011", ts1.strftime("%Y"));
        ATF_REQUIRE_EQ("2011", ts2.strftime("%Y"));
        ATF_REQUIRE_EQ("2012", ts3.strftime("%Y"));
    }
    ATF_REQUIRE_EQ("2011", ts1.strftime("%Y"));
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__now);
ATF_TEST_CASE_BODY(timestamp__now)
{
    // This test is might fail if we happen to run at the crossing of one
    // day to the other and the two measures we pick of the current time
    // differ.  This is so unlikely that I haven't bothered to do this in any
    // other way.

    const time_t just_before = ::time(NULL);
    const datetime::timestamp now = datetime::timestamp::now();

    ::tm data;
    char buf[1024];
    ATF_REQUIRE(::gmtime_r(&just_before, &data) != 0);
    ATF_REQUIRE(::strftime(buf, sizeof(buf), "%Y-%m-%d", &data) != 0);
    ATF_REQUIRE_EQ(buf, now.strftime("%Y-%m-%d"));

    ATF_REQUIRE(now.strftime("%Z") == "GMT" || now.strftime("%Z") == "UTC");
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__strftime);
ATF_TEST_CASE_BODY(timestamp__strftime)
{
    const datetime::timestamp ts1 = datetime::timestamp::from_values(
        2010, 12, 10, 8, 45, 50);
    ATF_REQUIRE_EQ("2010-12-10", ts1.strftime("%Y-%m-%d"));
    ATF_REQUIRE_EQ("08:45:50", ts1.strftime("%H:%M:%S"));

    const datetime::timestamp ts2 = datetime::timestamp::from_values(
        2011, 2, 16, 19, 15, 30);
    ATF_REQUIRE_EQ("2011-02-16T19:15:30", ts2.strftime("%Y-%m-%dT%H:%M:%S"));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, delta__defaults);
    ATF_ADD_TEST_CASE(tcs, delta__overrides);
    ATF_ADD_TEST_CASE(tcs, delta__equals);

    ATF_ADD_TEST_CASE(tcs, timestamp__copy);
    ATF_ADD_TEST_CASE(tcs, timestamp__now);
    ATF_ADD_TEST_CASE(tcs, timestamp__strftime);
}
