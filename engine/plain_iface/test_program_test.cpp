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

#include "engine/plain_iface/test_program.hpp"

#include <atf-c++.hpp>

#include "utils/fs/path.hpp"
#include "utils/optional.ipp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace plain_iface = engine::plain_iface;

using utils::none;


ATF_TEST_CASE_WITHOUT_HEAD(ctor__no_timeout);
ATF_TEST_CASE_BODY(ctor__no_timeout)
{
    const plain_iface::test_program test_program(fs::path("program"),
                                                 fs::path("root"),
                                                 "test-suite", none);
    ATF_REQUIRE_EQ("program", test_program.relative_path().str());
    ATF_REQUIRE_EQ("root", test_program.root().str());
    ATF_REQUIRE_EQ("test-suite", test_program.test_suite_name());
    ATF_REQUIRE(datetime::delta(300, 0) == test_program.timeout());
}


ATF_TEST_CASE_WITHOUT_HEAD(ctor__with_timeout);
ATF_TEST_CASE_BODY(ctor__with_timeout)
{
    const plain_iface::test_program test_program(
        fs::path("program"), fs::path("root"), "test-suite",
        utils::make_optional(datetime::delta(10, 3)));
    ATF_REQUIRE_EQ("program", test_program.relative_path().str());
    ATF_REQUIRE_EQ("root", test_program.root().str());
    ATF_REQUIRE_EQ("test-suite", test_program.test_suite_name());
    ATF_REQUIRE(datetime::delta(10, 3) == test_program.timeout());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_cases);
ATF_TEST_CASE_BODY(test_cases)
{
    const plain_iface::test_program test_program(fs::path("program"),
                                                 fs::path("root"),
                                                 "test-suite", none);
    const engine::test_cases_vector test_cases(test_program.test_cases());
    ATF_REQUIRE_EQ(1, test_cases.size());

    const engine::base_test_case* main_test_case = test_cases[0].get();
    ATF_REQUIRE(&test_program == &main_test_case->test_program());
    ATF_REQUIRE_EQ("main", main_test_case->name());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, ctor__no_timeout);
    ATF_ADD_TEST_CASE(tcs, ctor__with_timeout);
    ATF_ADD_TEST_CASE(tcs, test_cases);
}
