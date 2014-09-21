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

#include "model/test_case.hpp"

#include <sstream>

#include <atf-c++.hpp>

#include "model/metadata.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"

namespace fs = utils::fs;


ATF_TEST_CASE_WITHOUT_HEAD(test_case__ctor_and_getters)
ATF_TEST_CASE_BODY(test_case__ctor_and_getters)
{
    const model::metadata md = model::metadata_builder()
        .add_custom("first", "value")
        .build();
    const model::test_program test_program(
        "mock", fs::path("abc"), fs::path("unused-root"),
        "unused-suite-name", model::metadata_builder().build());
    const model::test_case test_case("mock", test_program, "foo", md);
    ATF_REQUIRE_EQ(&test_program, &test_case.container_test_program());
    ATF_REQUIRE_EQ("foo", test_case.name());
    ATF_REQUIRE(md == test_case.get_metadata());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__fake_result)
ATF_TEST_CASE_BODY(test_case__fake_result)
{
    const model::test_result result(model::test_result_skipped,
                                    "Some reason");
    const model::test_program test_program(
        "mock", fs::path("abc"), fs::path("unused-root"),
        "unused-suite-name", model::metadata_builder().build());
    const model::test_case test_case("mock", test_program, "__foo__",
                                      "Some description", result);
    ATF_REQUIRE_EQ(&test_program, &test_case.container_test_program());
    ATF_REQUIRE_EQ("__foo__", test_case.name());
    ATF_REQUIRE(result == test_case.fake_result().get());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__operators_eq_and_ne__copy);
ATF_TEST_CASE_BODY(test_case__operators_eq_and_ne__copy)
{
    const model::test_program tp(
        "plain", fs::path("non-existent"), fs::path("."), "suite-name",
        model::metadata_builder().build());

    const model::test_case tc1("plain", tp, "name",
                                model::metadata_builder().build());
    const model::test_case tc2 = tc1;
    ATF_REQUIRE(  tc1 == tc2);
    ATF_REQUIRE(!(tc1 != tc2));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__operators_eq_and_ne__not_copy);
ATF_TEST_CASE_BODY(test_case__operators_eq_and_ne__not_copy)
{
    const std::string base_interface("plain");
    const model::test_program base_tp(
        "plain", fs::path("non-existent"), fs::path("."), "suite-name",
        model::metadata_builder().build());
    const std::string base_name("name");
    const model::metadata base_metadata = model::metadata_builder()
        .add_custom("X-foo", "bar")
        .build();

    const model::test_case base_tc(base_interface, base_tp, base_name,
                                    base_metadata);

    // Construct with all same values.
    {
        const model::test_case other_tc(base_interface, base_tp, base_name,
                                        base_metadata);

        ATF_REQUIRE(  base_tc == other_tc);
        ATF_REQUIRE(!(base_tc != other_tc));
    }

    // Different interface.
    {
        const model::test_case other_tc("atf", base_tp, base_name,
                                         base_metadata);

        ATF_REQUIRE(!(base_tc == other_tc));
        ATF_REQUIRE(  base_tc != other_tc);
    }

    // Different test program, different identifier.
    {
        const model::test_program other_tp(
            "plain", fs::path("another-name"), fs::path("."), "suite2-name",
        model::metadata_builder().build());
        const model::test_case other_tc(base_interface, other_tp, base_name,
                                         base_metadata);

        ATF_REQUIRE(!(base_tc == other_tc));
        ATF_REQUIRE(  base_tc != other_tc);
    }

    // Different test program, same identifier.  Cannot be detected!
    {
        const model::test_program other_tp(
            "plain", fs::path("non-existent"), fs::path("."), "suite2-name",
        model::metadata_builder().build());
        const model::test_case other_tc(base_interface, other_tp, base_name,
                                         base_metadata);

        ATF_REQUIRE(  base_tc == other_tc);
        ATF_REQUIRE(!(base_tc != other_tc));
    }

    // Different name.
    {
        const model::test_case other_tc(base_interface, base_tp, "other",
                                         base_metadata);

        ATF_REQUIRE(!(base_tc == other_tc));
        ATF_REQUIRE(  base_tc != other_tc);
    }

    // Different metadata.
    {
        const model::test_case other_tc(base_interface, base_tp, base_name,
                                         model::metadata_builder().build());

        ATF_REQUIRE(!(base_tc == other_tc));
        ATF_REQUIRE(  base_tc != other_tc);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__output);
ATF_TEST_CASE_BODY(test_case__output)
{
    const model::test_program tp(
        "plain", fs::path("non-existent"), fs::path("."), "suite-name",
        model::metadata_builder().build());

    const model::test_case tc1(
        "plain", tp, "the-name", model::metadata_builder()
        .add_allowed_platform("foo").add_custom("X-bar", "baz").build());
    std::ostringstream str;
    str << tc1;
    ATF_REQUIRE_EQ(
        "test_case{interface='plain', name='the-name', "
        "metadata=metadata{allowed_architectures='', allowed_platforms='foo', "
        "custom.X-bar='baz', description='', has_cleanup='false', "
        "required_configs='', required_files='', required_memory='0', "
        "required_programs='', required_user='', timeout='300'}}",
        str.str());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, test_case__ctor_and_getters);
    ATF_ADD_TEST_CASE(tcs, test_case__fake_result);

    ATF_ADD_TEST_CASE(tcs, test_case__operators_eq_and_ne__copy);
    ATF_ADD_TEST_CASE(tcs, test_case__operators_eq_and_ne__not_copy);

    ATF_ADD_TEST_CASE(tcs, test_case__output);
}
