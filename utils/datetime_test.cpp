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


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, delta__defaults);
    ATF_ADD_TEST_CASE(tcs, delta__overrides);
    ATF_ADD_TEST_CASE(tcs, delta__equals);
}
