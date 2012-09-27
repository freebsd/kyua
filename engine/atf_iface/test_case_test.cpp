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
#include "utils/units.hpp"

namespace atf_iface = engine::atf_iface;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace units = utils::units;
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
    ATF_REQUIRE_EQ("Some description", test_case.description());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__from_properties__defaults)
ATF_TEST_CASE_BODY(test_case__from_properties__defaults)
{
    const mock_test_program test_program(fs::path("program"));
    const engine::properties_map properties;

    const atf_iface::test_case test_case =
        atf_iface::test_case::from_properties(test_program, "test-case",
                                              properties);

    ATF_REQUIRE_EQ(&test_program, &test_case.test_program());
    ATF_REQUIRE_EQ("test-case", test_case.name());
    ATF_REQUIRE(!test_case.has_cleanup());
    ATF_REQUIRE(engine::default_timeout == test_case.timeout());
    ATF_REQUIRE(test_case.allowed_architectures().empty());
    ATF_REQUIRE(test_case.allowed_platforms().empty());
    ATF_REQUIRE(test_case.required_configs().empty());
    ATF_REQUIRE(test_case.required_files().empty());
    ATF_REQUIRE(0 == test_case.required_memory());
    ATF_REQUIRE(test_case.required_programs().empty());
    ATF_REQUIRE(test_case.required_user().empty());
    ATF_REQUIRE(test_case.user_metadata().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__from_properties__override_all)
ATF_TEST_CASE_BODY(test_case__from_properties__override_all)
{
    const mock_test_program test_program(fs::path("program"));
    engine::properties_map properties;
    properties["descr"] = "Some text";
    properties["has.cleanup"] = "true";
    properties["require.arch"] = "i386 x86_64";
    properties["require.config"] = "var1 var2 var3";
    properties["require.files"] = "/file1 /dir/file2";
    properties["require.machine"] = "amd64";
    properties["require.memory"] = "1m";
    properties["require.progs"] = "/bin/ls svn";
    properties["require.user"] = "root";
    properties["timeout"] = "123";
    properties["X-foo"] = "value1";
    properties["X-bar"] = "value2";
    properties["X-baz-www"] = "value3";

    const atf_iface::test_case test_case =
        atf_iface::test_case::from_properties(test_program, "test-case",
                                              properties);

    ATF_REQUIRE_EQ(&test_program, &test_case.test_program());
    ATF_REQUIRE_EQ("test-case", test_case.name());
    ATF_REQUIRE(test_case.has_cleanup());
    ATF_REQUIRE(datetime::delta(123, 0) == test_case.timeout());
    ATF_REQUIRE_EQ(2, test_case.allowed_architectures().size());
    ATF_REQUIRE_IN("i386", test_case.allowed_architectures());
    ATF_REQUIRE_IN("x86_64", test_case.allowed_architectures());
    ATF_REQUIRE_EQ(1, test_case.allowed_platforms().size());
    ATF_REQUIRE_IN("amd64", test_case.allowed_platforms());
    ATF_REQUIRE_EQ(3, test_case.required_configs().size());
    ATF_REQUIRE_IN("var1", test_case.required_configs());
    ATF_REQUIRE_IN("var2", test_case.required_configs());
    ATF_REQUIRE_IN("var3", test_case.required_configs());
    ATF_REQUIRE_EQ(2, test_case.required_files().size());
    ATF_REQUIRE_IN(fs::path("/file1"), test_case.required_files());
    ATF_REQUIRE_IN(fs::path("/dir/file2"), test_case.required_files());
    ATF_REQUIRE_EQ(units::bytes::parse("1m"), test_case.required_memory());
    ATF_REQUIRE_EQ(2, test_case.required_programs().size());
    ATF_REQUIRE_IN(fs::path("/bin/ls"), test_case.required_programs());
    ATF_REQUIRE_IN(fs::path("svn"), test_case.required_programs());
    ATF_REQUIRE_EQ("root", test_case.required_user());
    ATF_REQUIRE_EQ(3, test_case.user_metadata().size());
    ATF_REQUIRE_EQ("value1", test_case.user_metadata().find("X-foo")->second);
    ATF_REQUIRE_EQ("value2", test_case.user_metadata().find("X-bar")->second);
    ATF_REQUIRE_EQ("value3", test_case.user_metadata().find(
                       "X-baz-www")->second);
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__from_properties__unknown)
ATF_TEST_CASE_BODY(test_case__from_properties__unknown)
{
    const mock_test_program test_program(fs::path("program"));
    engine::properties_map properties;
    properties["foobar"] = "Some text";

    ATF_REQUIRE_THROW_RE(engine::format_error, "Unknown.*property.*'foobar'",
        atf_iface::test_case::from_properties(test_program, "test-case",
                                              properties));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__all_properties__none)
ATF_TEST_CASE_BODY(test_case__all_properties__none)
{
    const mock_test_program test_program(fs::path("program"));
    engine::properties_map in_properties;
    engine::properties_map exp_properties;

    ATF_REQUIRE(exp_properties == atf_iface::test_case::from_properties(
        test_program, "test-case", in_properties).all_properties());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__all_properties__only_user)
ATF_TEST_CASE_BODY(test_case__all_properties__only_user)
{
    const mock_test_program test_program(fs::path("program"));

    engine::properties_map in_properties;
    in_properties["X-foo"] = "bar";
    in_properties["X-another-var"] = "This is a string";

    engine::properties_map exp_properties = in_properties;

    ATF_REQUIRE(exp_properties == atf_iface::test_case::from_properties(
        test_program, "test-case", in_properties).all_properties());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__all_properties__all)
ATF_TEST_CASE_BODY(test_case__all_properties__all)
{
    const mock_test_program test_program(fs::path("program"));

    engine::properties_map in_properties;
    in_properties["descr"] = "Some text that won't be sorted";
    in_properties["has.cleanup"] = "true";
    in_properties["require.arch"] = "i386 x86_64 macppc";
    in_properties["require.config"] = "var1 var3 var2";
    in_properties["require.machine"] = "amd64";
    in_properties["require.progs"] = "/bin/ls svn";
    in_properties["require.user"] = "root";
    in_properties["timeout"] = "123";
    in_properties["X-foo"] = "value1";
    in_properties["X-bar"] = "value2";
    in_properties["X-baz-www"] = "value3";

    engine::properties_map exp_properties = in_properties;
    // Ensure multi-word properties are sorted.
    exp_properties["require.arch"] = "i386 macppc x86_64";
    exp_properties["require.config"] = "var1 var2 var3";

    ATF_REQUIRE(exp_properties == atf_iface::test_case::from_properties(
        test_program, "test-case", in_properties).all_properties());
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
    ATF_ADD_TEST_CASE(tcs, test_case__from_properties__defaults);
    ATF_ADD_TEST_CASE(tcs, test_case__from_properties__override_all);
    ATF_ADD_TEST_CASE(tcs, test_case__from_properties__unknown);
    ATF_ADD_TEST_CASE(tcs, test_case__all_properties__none);
    ATF_ADD_TEST_CASE(tcs, test_case__all_properties__only_user);
    ATF_ADD_TEST_CASE(tcs, test_case__all_properties__all);
    ATF_ADD_TEST_CASE(tcs, test_case__run__fake);
}
