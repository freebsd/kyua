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

#include "engine/atf_iface/test_case.hpp"

#include <set>

#include <atf-c++.hpp>

#include "engine/exceptions.hpp"
#include "engine/test_program.hpp"
#include "engine/test_result.hpp"
#include "engine/user_files/config.hpp"
#include "utils/defs.hpp"
#include "utils/fs/path.hpp"
#include "utils/sanity.hpp"

namespace atf_iface = engine::atf_iface;
namespace fs = utils::fs;
namespace user_files = engine::user_files;


namespace {


/// Hooks to ensure that there is no stdout/stderr output.
class ensure_silent_hooks : public engine::test_case_hooks {
public:
    /// Fails the test case if called.
    ///
    /// \param unused_file Path to the stdout of the test case.
    void
    got_stdout(const fs::path& UTILS_UNUSED_PARAM(file))
    {
        ATF_FAIL("got_stdout() should not have been called");
    }

    /// Fails the test case if called.
    ///
    /// \param unused_file Path to the stderr of the test case.
    void
    got_stderr(const fs::path& UTILS_UNUSED_PARAM(file))
    {
        ATF_FAIL("got_stderr() should not have been called");
    }
};


/// Fake implementation of a test program.
class mock_test_program : public engine::base_test_program {
public:
    /// Constructs a new test program.
    ///
    /// Both the test suite root and the test suite name are fixed and
    /// supposedly unused in this module.
    ///
    /// \param binary_ The name of the test program binary.
    /// \param test_suite_name_ The name of the test suite, if necessary.
    mock_test_program(const fs::path& binary_,
                      const std::string& test_suite_name_ = "unused-suite") :
        base_test_program("mock", binary_, fs::path("unused-root"),
                          test_suite_name_)
    {
    }
};


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(test_case__ctor_and_getters)
ATF_TEST_CASE_BODY(test_case__ctor_and_getters)
{
    const mock_test_program test_program(fs::path("bin"));

    engine::metadata_builder mdbuilder;
    mdbuilder.set_string("allowed_platforms", "foo bar baz");

    const engine::metadata md = mdbuilder.build();
    const atf_iface::test_case test_case(test_program, "name", md);
    ATF_REQUIRE_EQ(&test_program, &test_case.test_program());
    ATF_REQUIRE_EQ("name", test_case.name());
    ATF_REQUIRE(md.to_properties() == test_case.get_metadata().to_properties());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__fake_ctor_and_getters)
ATF_TEST_CASE_BODY(test_case__fake_ctor_and_getters)
{
    const mock_test_program test_program(fs::path("bin"));
    const atf_iface::test_case test_case(
        test_program, "__internal_name__", "Some description",
        engine::test_result(engine::test_result::passed));

    ATF_REQUIRE_EQ(&test_program, &test_case.test_program());
    ATF_REQUIRE_EQ("__internal_name__", test_case.name());
    ATF_REQUIRE_EQ("Some description", test_case.get_metadata().description());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__run__fake)
ATF_TEST_CASE_BODY(test_case__run__fake)
{
    const engine::test_result result(engine::test_result::skipped, "Hello!");

    const mock_test_program test_program(fs::path("program"));
    const atf_iface::test_case test_case(
        test_program, "__internal_name__", "Some description", result);

    ensure_silent_hooks hooks;
    ATF_REQUIRE(result == engine::run_test_case(
                    &test_case, user_files::empty_config(), hooks));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, test_case__ctor_and_getters);
    ATF_ADD_TEST_CASE(tcs, test_case__fake_ctor_and_getters);
    ATF_ADD_TEST_CASE(tcs, test_case__run__fake);
}
