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

#include <set>
#include <typeinfo>

#include <atf-c++.hpp>

#include "engine/results.ipp"
#include "engine/test_case.hpp"
#include "engine/test_program.hpp"
#include "engine/user_files/config.hpp"
#include "utils/sanity.hpp"

namespace fs = utils::fs;
namespace results = engine::results;
namespace user_files = engine::user_files;

using utils::optional;


namespace {


/// Fake configuration.
static const user_files::config mock_config(
    "mock-architecture", "mock-platform", utils::none,
    user_files::test_suites_map());


/// Fake implementation of a test program.
class mock_test_program : public engine::base_test_program {
public:
    /// Constructs a new test program.
    ///
    /// Both the test suite root and the test suite name are fixed and
    /// supposedly unused in this module.
    ///
    /// \param binary_ The name of the test program binary.
    mock_test_program(const fs::path& binary_) :
        base_test_program(binary_, fs::path("unused-root"), "unused-suite-name")
    {
    }


    /// Gets the list of test cases from the test program.
    ///
    /// \return Nothing; this method is not supposed to be called.
    engine::test_cases_vector
    load_test_cases(void) const
    {
        UNREACHABLE;
    }
};


// Fake implementation of a test case.
class mock_test_case : public engine::base_test_case {
    /// Gets the collection of metadata properties of the test case.
    ///
    /// \return A static collection of properties for testing purposes.
    engine::properties_map
    get_all_properties(void) const
    {
        engine::properties_map properties;
        properties["first"] = "value";
        return properties;
    }

    /// Fakes the execution of a test case.
    ///
    /// \param config The run-time configuration.  Must be mock_config.
    ///
    /// \return A static result for testing purposes.
    results::result_ptr
    execute(const user_files::config& config,
            const optional< fs::path >& UTILS_UNUSED_PARAM(stdout_path),
            const optional< fs::path >& UTILS_UNUSED_PARAM(stderr_path)) const
    {
        if (&config != &mock_config)
            throw std::runtime_error("Invalid config object");
        return results::make_result(results::skipped("A test result"));
    }

public:
    /// Constructs a new test case.
    ///
    /// \param test_program_ The test program this test case belongs to.
    /// \param name_ The name of the test case within the test program.
    mock_test_case(const engine::base_test_program& test_program_,
                   const std::string& name_) :
        base_test_case(test_program_, name_)
    {
    }
};


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(test_case_id__public_fields)
ATF_TEST_CASE_BODY(test_case_id__public_fields)
{
    const engine::test_case_id id(fs::path("program"), "name");
    ATF_REQUIRE_EQ(fs::path("program"), id.program);
    ATF_REQUIRE_EQ("name", id.name);
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case_id__str)
ATF_TEST_CASE_BODY(test_case_id__str)
{
    const engine::test_case_id id(fs::path("dir/program"), "case1");
    ATF_REQUIRE_EQ("dir/program:case1", id.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case_id__operator_lt)
ATF_TEST_CASE_BODY(test_case_id__operator_lt)
{
    ATF_REQUIRE(engine::test_case_id(fs::path("a"), "b") <
                engine::test_case_id(fs::path("c"), "a"));

    ATF_REQUIRE(engine::test_case_id(fs::path("a"), "b") <
                engine::test_case_id(fs::path("a"), "c"));

    ATF_REQUIRE(!(engine::test_case_id(fs::path("a"), "b") <
                  engine::test_case_id(fs::path("a"), "a")));

    ATF_REQUIRE(!(engine::test_case_id(fs::path("b"), "a") <
                  engine::test_case_id(fs::path("a"), "a")));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case_id__operator_eq)
ATF_TEST_CASE_BODY(test_case_id__operator_eq)
{
    ATF_REQUIRE(engine::test_case_id(fs::path("a"), "b") ==
                engine::test_case_id(fs::path("a"), "b"));

    ATF_REQUIRE(!(engine::test_case_id(fs::path("a"), "a") ==
                  engine::test_case_id(fs::path("a"), "b")));

    ATF_REQUIRE(!(engine::test_case_id(fs::path("a"), "b") ==
                  engine::test_case_id(fs::path("b"), "b")));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case_id__use_as_key)
ATF_TEST_CASE_BODY(test_case_id__use_as_key)
{
    std::set< engine::test_case_id > ids;
    const engine::test_case_id id(fs::path("foo"), "bar");
    ids.insert(id);
    ATF_REQUIRE(ids.find(id) != ids.end());
    ATF_REQUIRE(ids.find(engine::test_case_id(fs::path("foo"), "b")) ==
                ids.end());
    ATF_REQUIRE(ids.find(engine::test_case_id(fs::path("f"), "bar")) ==
                ids.end());
}


ATF_TEST_CASE_WITHOUT_HEAD(base_test_case__ctor_and_getters)
ATF_TEST_CASE_BODY(base_test_case__ctor_and_getters)
{
    const mock_test_program test_program(fs::path("abc"));
    const mock_test_case test_case(test_program, "foo");
    ATF_REQUIRE_EQ(&test_program, &test_case.test_program());
    ATF_REQUIRE_EQ("foo", test_case.name());
}


ATF_TEST_CASE_WITHOUT_HEAD(base_test_case__identifier)
ATF_TEST_CASE_BODY(base_test_case__identifier)
{
    const mock_test_program test_program(fs::path("foo"));
    const mock_test_case test_case(test_program, "bar");
    ATF_REQUIRE(engine::test_case_id(fs::path("foo"), "bar") ==
                test_case.identifier());
}


ATF_TEST_CASE_WITHOUT_HEAD(base_test_case__all_properties__delegate)
ATF_TEST_CASE_BODY(base_test_case__all_properties__delegate)
{
    const mock_test_program test_program(fs::path("foo"));
    const mock_test_case test_case(test_program, "bar");

    engine::properties_map exp_properties;
    exp_properties["first"] = "value";
    ATF_REQUIRE(exp_properties == test_case.all_properties());
}


ATF_TEST_CASE_WITHOUT_HEAD(base_test_case__run__delegate)
ATF_TEST_CASE_BODY(base_test_case__run__delegate)
{
    const mock_test_program test_program(fs::path("foo"));
    const mock_test_case test_case(test_program, "bar");

    const results::result_ptr result = test_case.run(mock_config);
    ATF_REQUIRE(typeid(*result) == typeid(results::skipped));
    const results::skipped* typed_result =
        dynamic_cast< const results::skipped* >(result.get());
    ATF_REQUIRE_EQ("A test result", typed_result->reason);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, test_case_id__public_fields);
    ATF_ADD_TEST_CASE(tcs, test_case_id__str);
    ATF_ADD_TEST_CASE(tcs, test_case_id__operator_lt);
    ATF_ADD_TEST_CASE(tcs, test_case_id__operator_eq);
    ATF_ADD_TEST_CASE(tcs, test_case_id__use_as_key);

    ATF_ADD_TEST_CASE(tcs, base_test_case__ctor_and_getters);
    ATF_ADD_TEST_CASE(tcs, base_test_case__identifier);
    ATF_ADD_TEST_CASE(tcs, base_test_case__all_properties__delegate);
    ATF_ADD_TEST_CASE(tcs, base_test_case__run__delegate);
}
