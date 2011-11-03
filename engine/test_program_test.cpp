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

#include "engine/atf_iface/test_case.hpp"
#include "engine/test_program.hpp"
#include "utils/fs/path.hpp"

namespace atf_iface = engine::atf_iface;
namespace fs = utils::fs;


namespace {


/// Fake implementation of a test program.
class mock_test_program : public engine::base_test_program {
public:
    /// Number of times the load_test_cases() method has been called.
    mutable int loads;


    /// Constructs a new test program.
    ///
    /// \param binary_ The name of the test program binary relative to root_.
    /// \param root_ The root of the test suite containing the test program.
    /// \param test_suite_name_ The name of the test suite this program belongs
    ///     to.
    mock_test_program(const fs::path& binary_, const fs::path& root_,
                      const std::string& test_suite_name_) :
        base_test_program(binary_, root_, test_suite_name_),
        loads(0)
    {
    }


    /// Gets the list of test cases from the test program.
    ///
    /// \return The list of test cases provided by the test program.
    engine::test_cases_vector
    load_test_cases(void) const
    {
        loads++;

        engine::test_cases_vector loaded_test_cases;

        const atf_iface::test_case test_case =
            atf_iface::test_case::from_properties(*this, "foo",
                                                   engine::properties_map());
        loaded_test_cases.push_back(engine::test_case_ptr(
            new atf_iface::test_case(test_case)));
        return loaded_test_cases;
    }
};


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(ctor_and_getters);
ATF_TEST_CASE_BODY(ctor_and_getters)
{
    const mock_test_program test_program(fs::path("binary"), fs::path("root"),
                                         "suite-name");
    ATF_REQUIRE_EQ(fs::path("binary"), test_program.relative_path());
    ATF_REQUIRE_EQ(fs::path("root/binary"), test_program.absolute_path());
    ATF_REQUIRE_EQ(fs::path("root"), test_program.root());
    ATF_REQUIRE_EQ("suite-name", test_program.test_suite_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(unique_address);
ATF_TEST_CASE_BODY(unique_address)
{
    const mock_test_program tp1(fs::path("binary"), fs::path("root"),
                                "suite-name");
    {
        const mock_test_program tp2 = tp1;
        const mock_test_program tp3(fs::path("binary"), fs::path("root"),
                                    "suite-name");
        ATF_REQUIRE(tp1.unique_address() == tp2.unique_address());
        ATF_REQUIRE(tp1.unique_address() != tp3.unique_address());
        ATF_REQUIRE(tp2.unique_address() != tp3.unique_address());
    }
    ATF_REQUIRE(tp1.unique_address() == tp1.unique_address());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_cases__get);
ATF_TEST_CASE_BODY(test_cases__get)
{
    const mock_test_program test_program(fs::path("binary"), fs::path("root"),
                                         "suite-name");
    const engine::test_cases_vector& test_cases = test_program.test_cases();
    ATF_REQUIRE_EQ(1, test_cases.size());
    ATF_REQUIRE_EQ("binary:foo", test_cases[0]->identifier().str());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_cases__cached);
ATF_TEST_CASE_BODY(test_cases__cached)
{
    const mock_test_program test_program(fs::path("binary"), fs::path("root"),
                                         "suite-name");
    ATF_REQUIRE_EQ(0, test_program.loads);
    (void)test_program.test_cases();
    ATF_REQUIRE_EQ(1, test_program.loads);
    (void)test_program.test_cases();
    ATF_REQUIRE_EQ(1, test_program.loads);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, ctor_and_getters);
    ATF_ADD_TEST_CASE(tcs, unique_address);
    ATF_ADD_TEST_CASE(tcs, test_cases__get);
    ATF_ADD_TEST_CASE(tcs, test_cases__cached);
}
