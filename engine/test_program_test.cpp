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

#include <atf-c++.hpp>

#include "engine/atf_iface/test_case.hpp"
#include "engine/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"

namespace atf_iface = engine::atf_iface;
namespace fs = utils::fs;


ATF_TEST_CASE_WITHOUT_HEAD(ctor_and_getters);
ATF_TEST_CASE_BODY(ctor_and_getters)
{
    const engine::metadata md = engine::metadata_builder()
        .add_custom("foo", "bar")
        .build();
    const engine::test_program test_program(
        "mock", fs::path("binary"), fs::path("root"), "suite-name", md);
    ATF_REQUIRE_EQ("mock", test_program.interface_name());
    ATF_REQUIRE_EQ(fs::path("binary"), test_program.relative_path());
    ATF_REQUIRE_EQ(fs::current_path() / "root/binary",
                   test_program.absolute_path());
    ATF_REQUIRE_EQ(fs::path("root"), test_program.root());
    ATF_REQUIRE_EQ("suite-name", test_program.test_suite_name());
    ATF_REQUIRE(md.to_properties() ==
                test_program.get_metadata().to_properties());
}


ATF_TEST_CASE_WITHOUT_HEAD(find__ok);
ATF_TEST_CASE_BODY(find__ok)
{
    const engine::test_program test_program(
        "mock", fs::path("binary"), fs::path("root"), "suite-name",
        engine::metadata_builder().build());
    expect_death("Cannot implement mock test case without TestersDesign");
    const engine::test_case_ptr test_case = test_program.find("foo");
    ATF_REQUIRE_EQ(fs::path("binary"),
                   test_case->container_test_program().relative_path());
    ATF_REQUIRE_EQ("foo", test_case->name());
}


ATF_TEST_CASE_WITHOUT_HEAD(find__missing);
ATF_TEST_CASE_BODY(find__missing)
{
    const engine::test_program test_program(
        "mock", fs::path("binary"), fs::path("root"), "suite-name",
        engine::metadata_builder().build());
    expect_death("Cannot implement mock test case without TestersDesign");
    ATF_REQUIRE_THROW_RE(engine::not_found_error, "case.*abc.*program.*binary",
                         test_program.find("abc"));
    //ATF_REQUIRE_EQ(1, test_program.loads);
}


ATF_TEST_CASE_WITHOUT_HEAD(test_cases__get);
ATF_TEST_CASE_BODY(test_cases__get)
{
    const engine::test_program test_program(
        "mock", fs::path("binary"), fs::path("root"), "suite-name",
        engine::metadata_builder().build());
    expect_death("Cannot implement mock test case without TestersDesign");
    const engine::test_cases_vector& test_cases = test_program.test_cases();
    ATF_REQUIRE_EQ(1, test_cases.size());
    ATF_REQUIRE_EQ(fs::path("binary"),
                   test_cases[0]->container_test_program().relative_path());
    ATF_REQUIRE_EQ("foo", test_cases[0]->name());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_cases__cached);
ATF_TEST_CASE_BODY(test_cases__cached)
{
    const engine::test_program test_program(
        "mock", fs::path("binary"), fs::path("root"), "suite-name",
        engine::metadata_builder().build());
    expect_death("Cannot implement mock test case without TestersDesign");
    //ATF_REQUIRE_EQ(0, test_program.loads);
    (void)test_program.test_cases();
    //ATF_REQUIRE_EQ(1, test_program.loads);
    (void)test_program.test_cases();
    //ATF_REQUIRE_EQ(1, test_program.loads);
}


ATF_TEST_CASE_WITHOUT_HEAD(test_cases__set__empty);
ATF_TEST_CASE_BODY(test_cases__set__empty)
{
    engine::test_program test_program(
        "mock", fs::path("binary"), fs::path("root"), "suite-name",
        engine::metadata_builder().build());

    //ATF_REQUIRE_EQ(0, test_program.loads);
    const engine::test_cases_vector exp_test_cases;
    test_program.set_test_cases(exp_test_cases);

    ATF_REQUIRE(exp_test_cases == test_program.test_cases());
    //ATF_REQUIRE_EQ(0, test_program.loads);
}


ATF_TEST_CASE_WITHOUT_HEAD(test_cases__set__some);
ATF_TEST_CASE_BODY(test_cases__set__some)
{
    engine::test_program test_program(
        "mock", fs::path("binary"), fs::path("root"), "suite-name",
        engine::metadata_builder().build());

    //ATF_REQUIRE_EQ(0, test_program.loads);
    engine::test_cases_vector exp_test_cases;
    const engine::test_case test_case("mock", test_program, "hello",
                                      engine::metadata_builder().build());
    exp_test_cases.push_back(engine::test_case_ptr(
        new engine::test_case(test_case)));
    test_program.set_test_cases(exp_test_cases);

    ATF_REQUIRE(exp_test_cases == test_program.test_cases());
    //ATF_REQUIRE_EQ(0, test_program.loads);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, ctor_and_getters);
    ATF_ADD_TEST_CASE(tcs, find__ok);
    ATF_ADD_TEST_CASE(tcs, find__missing);
    ATF_ADD_TEST_CASE(tcs, test_cases__get);
    ATF_ADD_TEST_CASE(tcs, test_cases__cached);
    ATF_ADD_TEST_CASE(tcs, test_cases__set__empty);
    ATF_ADD_TEST_CASE(tcs, test_cases__set__some);
}
