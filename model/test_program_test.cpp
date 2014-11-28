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

#include "model/test_program.hpp"

extern "C" {
#include <sys/stat.h>

#include <signal.h>
}

#include <sstream>

#include <atf-c++.hpp>

#include "model/exceptions.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_result.hpp"
#include "utils/env.hpp"
#include "utils/format/containers.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"

namespace fs = utils::fs;


ATF_TEST_CASE_WITHOUT_HEAD(ctor_and_getters);
ATF_TEST_CASE_BODY(ctor_and_getters)
{
    const model::metadata md = model::metadata_builder()
        .add_custom("foo", "bar")
        .build();
    model::test_cases_map tcs;
    tcs.insert(model::test_cases_map::value_type(
        "foo", model::test_case("foo", model::metadata_builder().build())));
    const model::test_program test_program(
        "mock", fs::path("binary"), fs::path("root"), "suite-name", md, tcs);

    ATF_REQUIRE_EQ("mock", test_program.interface_name());
    ATF_REQUIRE_EQ(fs::path("binary"), test_program.relative_path());
    ATF_REQUIRE_EQ(fs::current_path() / "root/binary",
                   test_program.absolute_path());
    ATF_REQUIRE_EQ(fs::path("root"), test_program.root());
    ATF_REQUIRE_EQ("suite-name", test_program.test_suite_name());
    ATF_REQUIRE_EQ(md, test_program.get_metadata());
    ATF_REQUIRE_EQ(tcs, test_program.test_cases());
}


ATF_TEST_CASE_WITHOUT_HEAD(find__ok);
ATF_TEST_CASE_BODY(find__ok)
{
    const model::test_program test_program = model::test_program_builder(
        "mock", fs::path("non-existent"), fs::path("."), "suite-name")
        .add_test_case("main")
        .build();

    const model::test_case exp_test_case(
        "main", model::metadata_builder().build());

    const model::test_case& test_case = test_program.find("main");
    ATF_REQUIRE_EQ(exp_test_case, test_case);
}


ATF_TEST_CASE_WITHOUT_HEAD(find__missing);
ATF_TEST_CASE_BODY(find__missing)
{
    const model::test_program test_program = model::test_program_builder(
        "mock", fs::path("non-existent"), fs::path("."), "suite-name")
        .add_test_case("main")
        .build();

    ATF_REQUIRE_THROW_RE(model::not_found_error,
                         "case.*abc.*program.*non-existent",
                         test_program.find("abc"));
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne__copy);
ATF_TEST_CASE_BODY(operators_eq_and_ne__copy)
{
    const model::test_program tp1(
        "plain", fs::path("non-existent"), fs::path("."), "suite-name",
        model::metadata_builder().build(),
        model::test_cases_map());
    const model::test_program tp2 = tp1;
    ATF_REQUIRE(  tp1 == tp2);
    ATF_REQUIRE(!(tp1 != tp2));
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne__not_copy);
ATF_TEST_CASE_BODY(operators_eq_and_ne__not_copy)
{
    const std::string base_interface("plain");
    const fs::path base_relative_path("the/test/program");
    const fs::path base_root("/the/root");
    const std::string base_test_suite("suite-name");
    const model::metadata base_metadata = model::metadata_builder()
        .add_custom("X-foo", "bar")
        .build();

    model::test_cases_map base_tcs;
    {
        const model::test_case tc1("main", model::metadata_builder().build());
        base_tcs.insert(model::test_cases_map::value_type(tc1.name(), tc1));
    }

    const model::test_program base_tp(
        base_interface, base_relative_path, base_root, base_test_suite,
        base_metadata, base_tcs);

    // Construct with all same values.
    {
        model::test_cases_map other_tcs;
        {
            const model::test_case tc1("main",
                                       model::metadata_builder().build());
            other_tcs.insert(model::test_cases_map::value_type(tc1.name(),
                                                               tc1));
        }

        const model::test_program other_tp(
            base_interface, base_relative_path, base_root, base_test_suite,
            base_metadata, other_tcs);

        ATF_REQUIRE(  base_tp == other_tp);
        ATF_REQUIRE(!(base_tp != other_tp));
    }

    // Different interface.
    {
        const model::test_program other_tp(
            "atf", base_relative_path, base_root, base_test_suite,
            base_metadata, base_tcs);

        ATF_REQUIRE(!(base_tp == other_tp));
        ATF_REQUIRE(  base_tp != other_tp);
    }

    // Different relative path.
    {
        const model::test_program other_tp(
            base_interface, fs::path("a/b/c"), base_root, base_test_suite,
            base_metadata, base_tcs);

        ATF_REQUIRE(!(base_tp == other_tp));
        ATF_REQUIRE(  base_tp != other_tp);
    }

    // Different root.
    {
        const model::test_program other_tp(
            base_interface, base_relative_path, fs::path("."), base_test_suite,
            base_metadata, base_tcs);

        ATF_REQUIRE(!(base_tp == other_tp));
        ATF_REQUIRE(  base_tp != other_tp);
    }

    // Different test suite.
    {
        const model::test_program other_tp(
            base_interface, base_relative_path, base_root, "different-suite",
            base_metadata, base_tcs);

        ATF_REQUIRE(!(base_tp == other_tp));
        ATF_REQUIRE(  base_tp != other_tp);
    }

    // Different metadata.
    {
        const model::test_program other_tp(
            base_interface, base_relative_path, base_root, base_test_suite,
            model::metadata_builder().build(), base_tcs);

        ATF_REQUIRE(!(base_tp == other_tp));
        ATF_REQUIRE(  base_tp != other_tp);
    }

    // Different test cases.
    {
        model::test_cases_map other_tcs;
        {
            const model::test_case tc1("foo",
                                       model::metadata_builder().build());
            other_tcs.insert(model::test_cases_map::value_type(tc1.name(),
                                                               tc1));
        }

        const model::test_program other_tp(
            base_interface, base_relative_path, base_root, base_test_suite,
            base_metadata, other_tcs);

        ATF_REQUIRE(!(base_tp == other_tp));
        ATF_REQUIRE(  base_tp != other_tp);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(output__no_test_cases);
ATF_TEST_CASE_BODY(output__no_test_cases)
{
    model::test_program tp(
        "plain", fs::path("binary/path"), fs::path("/the/root"), "suite-name",
        model::metadata_builder().add_allowed_architecture("a").build(),
        model::test_cases_map());

    std::ostringstream str;
    str << tp;
    ATF_REQUIRE_EQ(
        "test_program{interface='plain', binary='binary/path', "
        "root='/the/root', test_suite='suite-name', "
        "metadata=metadata{allowed_architectures='a', allowed_platforms='', "
        "description='', has_cleanup='false', "
        "required_configs='', required_disk_space='0', required_files='', "
        "required_memory='0', "
        "required_programs='', required_user='', timeout='300'}, "
        "test_cases=map()}",
        str.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(output__some_test_cases);
ATF_TEST_CASE_BODY(output__some_test_cases)
{
    const model::test_program tp = model::test_program_builder(
        "plain", fs::path("binary/path"), fs::path("/the/root"), "suite-name")
        .add_test_case("the-name",
                       model::metadata_builder()
                       .add_allowed_platform("foo")
                       .add_custom("X-bar", "baz")
                       .build())
        .add_test_case("another-name")
        .set_metadata(model::metadata_builder()
                      .add_allowed_architecture("a")
                      .build())
        .build();

    std::ostringstream str;
    str << tp;
    ATF_REQUIRE_EQ(
        "test_program{interface='plain', binary='binary/path', "
        "root='/the/root', test_suite='suite-name', "
        "metadata=metadata{allowed_architectures='a', allowed_platforms='', "
        "description='', has_cleanup='false', "
        "required_configs='', required_disk_space='0', required_files='', "
        "required_memory='0', "
        "required_programs='', required_user='', timeout='300'}, "
        "test_cases=map("
        "another-name=test_case{name='another-name', "
        "metadata=metadata{allowed_architectures='', allowed_platforms='', "
        "description='', has_cleanup='false', "
        "required_configs='', required_disk_space='0', required_files='', "
        "required_memory='0', "
        "required_programs='', required_user='', timeout='300'}}, "
        "the-name=test_case{name='the-name', "
        "metadata=metadata{allowed_architectures='', allowed_platforms='foo', "
        "custom.X-bar='baz', description='', has_cleanup='false', "
        "required_configs='', required_disk_space='0', required_files='', "
        "required_memory='0', "
        "required_programs='', required_user='', timeout='300'}})}",
        str.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(builder__defaults);
ATF_TEST_CASE_BODY(builder__defaults)
{
    const model::test_program expected(
        "mock", fs::path("non-existent"), fs::path("."), "suite-name",
        model::metadata_builder().build(), model::test_cases_map());

    const model::test_program built = model::test_program_builder(
        "mock", fs::path("non-existent"), fs::path("."), "suite-name")
        .build();

    ATF_REQUIRE_EQ(built, expected);
}


ATF_TEST_CASE_WITHOUT_HEAD(builder__overrides);
ATF_TEST_CASE_BODY(builder__overrides)
{
    const model::metadata md = model::metadata_builder()
        .add_custom("foo", "bar")
        .build();
    model::test_cases_map tcs;
    tcs.insert(model::test_cases_map::value_type(
        "first", model::test_case("first", model::metadata_builder().build())));
    tcs.insert(model::test_cases_map::value_type(
        "second", model::test_case("second", md)));
    const model::test_program expected(
        "mock", fs::path("binary"), fs::path("root"), "suite-name", md, tcs);

    const model::test_program built = model::test_program_builder(
        "mock", fs::path("binary"), fs::path("root"), "suite-name")
        .add_test_case("first")
        .add_test_case("second", md)
        .set_metadata(md)
        .build();

    ATF_REQUIRE_EQ(built, expected);
}


ATF_TEST_CASE_WITHOUT_HEAD(builder__ptr);
ATF_TEST_CASE_BODY(builder__ptr)
{
    const model::test_program expected(
        "mock", fs::path("non-existent"), fs::path("."), "suite-name",
        model::metadata_builder().build(), model::test_cases_map());

    const model::test_program_ptr built = model::test_program_builder(
        "mock", fs::path("non-existent"), fs::path("."), "suite-name")
        .build_ptr();

    ATF_REQUIRE_EQ(*built, expected);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, ctor_and_getters);
    ATF_ADD_TEST_CASE(tcs, find__ok);
    ATF_ADD_TEST_CASE(tcs, find__missing);

    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne__copy);
    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne__not_copy);

    ATF_ADD_TEST_CASE(tcs, output__no_test_cases);
    ATF_ADD_TEST_CASE(tcs, output__some_test_cases);

    ATF_ADD_TEST_CASE(tcs, builder__defaults);
    ATF_ADD_TEST_CASE(tcs, builder__overrides);
    ATF_ADD_TEST_CASE(tcs, builder__ptr);
}
