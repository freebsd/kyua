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

#include "engine/test_program.hpp"

extern "C" {
#include <sys/stat.h>

#include <signal.h>
}

#include <atf-c++.hpp>

#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "utils/env.hpp"
#include "utils/format/containers.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"

namespace fs = utils::fs;


namespace {


/// Creates a mock tester that receives a signal.
///
/// \param term_sig Signal to deliver to the tester.  If the tester does not
///     exit due to this reason, it exits with an arbitrary non-zero code.
static void
create_mock_tester_signal(const int term_sig)
{
    const std::string tester_name = "kyua-mock-tester";

    atf::utils::create_file(
        tester_name,
        F("#! /bin/sh\n"
          "kill -%s $$\n"
          "exit 0\n") % term_sig);
    ATF_REQUIRE(::chmod(tester_name.c_str(), 0755) != -1);

    utils::setenv("KYUA_TESTERSDIR", fs::current_path().str());
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(load_test_cases__get);
ATF_TEST_CASE_BODY(load_test_cases__get)
{
    model::test_program test_program(
        "plain", fs::path("non-existent"), fs::path("."), "suite-name",
        model::metadata_builder().build());
    engine::load_test_cases(test_program);
    const model::test_cases_vector& test_cases = test_program.test_cases();
    ATF_REQUIRE_EQ(1, test_cases.size());
    ATF_REQUIRE_EQ(fs::path("non-existent"),
                   test_cases[0]->container_test_program().relative_path());
    ATF_REQUIRE_EQ("main", test_cases[0]->name());
}


ATF_TEST_CASE_WITHOUT_HEAD(load_test_cases__some);
ATF_TEST_CASE_BODY(load_test_cases__some)
{
    model::test_program test_program(
        "plain", fs::path("non-existent"), fs::path("."), "suite-name",
        model::metadata_builder().build());

    model::test_cases_vector exp_test_cases;
    const model::test_case test_case("plain", test_program, "main",
                                     model::metadata_builder().build());
    exp_test_cases.push_back(model::test_case_ptr(
        new model::test_case(test_case)));
    test_program.set_test_cases(exp_test_cases);

    engine::load_test_cases(test_program);
    ATF_REQUIRE_EQ(exp_test_cases, test_program.test_cases());
}


ATF_TEST_CASE_WITHOUT_HEAD(load_test_cases__tester_fails);
ATF_TEST_CASE_BODY(load_test_cases__tester_fails)
{
    model::test_program test_program(
        "mock", fs::path("non-existent"), fs::path("."), "suite-name",
        model::metadata_builder().build());
    create_mock_tester_signal(SIGSEGV);

    engine::load_test_cases(test_program);
    const model::test_cases_vector& test_cases = test_program.test_cases();
    ATF_REQUIRE_EQ(1, test_cases.size());

    const model::test_case_ptr& test_case = test_cases[0];
    ATF_REQUIRE_EQ("__test_cases_list__", test_case->name());

    ATF_REQUIRE(test_case->fake_result());
    const model::test_result result = test_case->fake_result().get();
    ATF_REQUIRE(model::test_result::broken == result.type());
    ATF_REQUIRE_MATCH("Tester did not exit cleanly", result.reason());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, load_test_cases__get);
    ATF_ADD_TEST_CASE(tcs, load_test_cases__some);
    ATF_ADD_TEST_CASE(tcs, load_test_cases__tester_fails);
}
