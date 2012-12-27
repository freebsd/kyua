// Copyright 2012 Google Inc.
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

#include "engine/testers.hpp"

#include <atf-c++.hpp>

#include "engine/exceptions.hpp"
#include "utils/env.hpp"
#include "utils/fs/operations.hpp"

namespace fs = utils::fs;


ATF_TEST_CASE_WITHOUT_HEAD(tester_path__default);
ATF_TEST_CASE_BODY(tester_path__default)
{
    ATF_REQUIRE(atf::utils::file_exists(engine::tester_path("atf").str()));
    ATF_REQUIRE(atf::utils::file_exists(engine::tester_path("plain").str()));
}


ATF_TEST_CASE_WITHOUT_HEAD(tester_path__custom);
ATF_TEST_CASE_BODY(tester_path__custom)
{
    fs::mkdir(fs::path("testers"), 0755);
    atf::utils::create_file("testers/kyua-mock-tester", "Not a binary");

    utils::setenv("KYUA_TESTERSDIR", (fs::current_path() / "unknown").str());
    ATF_REQUIRE_THROW_RE(engine::error, "Unknown interface mock",
                         engine::tester_path("mock"));
    utils::setenv("KYUA_TESTERSDIR", (fs::current_path() / "testers").str());
    ATF_REQUIRE(atf::utils::file_exists(engine::tester_path("mock").str()));
}


ATF_TEST_CASE_WITHOUT_HEAD(tester_path__missing);
ATF_TEST_CASE_BODY(tester_path__missing)
{
    utils::setenv("KYUA_TESTERSDIR", fs::current_path().str());
    ATF_REQUIRE_THROW_RE(engine::error, "Unknown interface plain",
                         engine::tester_path("plain"));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, tester_path__default);
    ATF_ADD_TEST_CASE(tcs, tester_path__custom);
    ATF_ADD_TEST_CASE(tcs, tester_path__missing);
}
