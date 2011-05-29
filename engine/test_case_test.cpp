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

#include <atf-c++.hpp>

#include "engine/exceptions.hpp"
#include "engine/test_case.hpp"
#include "engine/user_files/config.hpp"
#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/passwd.hpp"
#include "utils/test_utils.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace user_files = engine::user_files;


namespace {


/// Fake configuration.
static const user_files::config mock_config(
    "mock-architecture", "mock-platform", utils::none,
    user_files::test_suites_map());


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(parse_bool__true)
ATF_TEST_CASE_BODY(parse_bool__true)
{
    ATF_REQUIRE(engine::detail::parse_bool("unused-name", "yes"));
    ATF_REQUIRE(engine::detail::parse_bool("unused-name", "true"));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_bool__false)
ATF_TEST_CASE_BODY(parse_bool__false)
{
    ATF_REQUIRE(!engine::detail::parse_bool("unused-name", "no"));
    ATF_REQUIRE(!engine::detail::parse_bool("unused-name", "false"));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_bool__invalid)
ATF_TEST_CASE_BODY(parse_bool__invalid)
{
    ATF_REQUIRE_THROW_RE(engine::format_error,
                         "value ''.*property 'a'",
                         engine::detail::parse_bool("a", ""));
    ATF_REQUIRE_THROW_RE(engine::format_error,
                         "value 'foo'.*property 'a'",
                         engine::detail::parse_bool("a", "foo"));
    ATF_REQUIRE_THROW_RE(engine::format_error,
                         "value 'True'.*property 'abcd'",
                         engine::detail::parse_bool("abcd", "True"));
    ATF_REQUIRE_THROW_RE(engine::format_error,
                         "value 'False'.*property 'name'",
                         engine::detail::parse_bool("name", "False"));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_list__empty)
ATF_TEST_CASE_BODY(parse_list__empty)
{
    ATF_REQUIRE_THROW_RE(engine::format_error, "empty.*property 'i-am-empty'",
                         engine::detail::parse_list("i-am-empty", ""));
    ATF_REQUIRE_THROW_RE(engine::format_error, "empty.*property 'i-am-empty'",
                         engine::detail::parse_list("i-am-empty", "    "));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_list__one_word)
ATF_TEST_CASE_BODY(parse_list__one_word)
{
    engine::strings_set words;

    words = engine::detail::parse_list("unused-name", "foo");
    ATF_REQUIRE_EQ(1, words.size());
    ATF_REQUIRE(words.find("foo") != words.end());

    words = engine::detail::parse_list("unused-name", "  foo");
    ATF_REQUIRE_EQ(1, words.size());
    ATF_REQUIRE(words.find("foo") != words.end());

    words = engine::detail::parse_list("unused-name", "foo  ");
    ATF_REQUIRE_EQ(1, words.size());
    ATF_REQUIRE(words.find("foo") != words.end());
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_list__many_words)
ATF_TEST_CASE_BODY(parse_list__many_words)
{
    engine::strings_set words;

    words = engine::detail::parse_list("unused-name", "foo bar baz");
    ATF_REQUIRE_EQ(3, words.size());
    ATF_REQUIRE(words.find("foo") != words.end());
    ATF_REQUIRE(words.find("bar") != words.end());
    ATF_REQUIRE(words.find("baz") != words.end());

    words = engine::detail::parse_list("unused-name", " foo  ba   b    ");
    ATF_REQUIRE_EQ(3, words.size());
    ATF_REQUIRE(words.find("foo") != words.end());
    ATF_REQUIRE(words.find("ba") != words.end());
    ATF_REQUIRE(words.find("b") != words.end());
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_ulong__ok)
ATF_TEST_CASE_BODY(parse_ulong__ok)
{
    ATF_REQUIRE_EQ(0, engine::detail::parse_ulong("unused-name", "0"));
    ATF_REQUIRE_EQ(312, engine::detail::parse_ulong("unused-name", "312"));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_ulong__empty)
ATF_TEST_CASE_BODY(parse_ulong__empty)
{
    ATF_REQUIRE_THROW_RE(engine::format_error, "empty.*property 'i-am-empty'",
                         engine::detail::parse_ulong("i-am-empty", ""));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_ulong__invalid)
ATF_TEST_CASE_BODY(parse_ulong__invalid)
{
    ATF_REQUIRE_THROW_RE(engine::format_error,
                         "value '  '.*property 'blanks'",
                         engine::detail::parse_ulong("blanks", "  "));

    ATF_REQUIRE_THROW_RE(engine::format_error,
                         "value '-3'.*property 'negative'",
                         engine::detail::parse_ulong("negative", "-3"));

    ATF_REQUIRE_THROW_RE(engine::format_error,
                         "value ' 123'.*property 'space-first'",
                         engine::detail::parse_ulong("space-first", " 123"));
    ATF_REQUIRE_THROW_RE(engine::format_error,
                         "value '123 '.*property 'space-last'",
                         engine::detail::parse_ulong("space-last", "123 "));

    ATF_REQUIRE_THROW_RE(engine::format_error,
                         "value 'z78'.*property 'alpha-first'",
                         engine::detail::parse_ulong("alpha-first", "z78"));
    ATF_REQUIRE_THROW_RE(engine::format_error,
                         "value '3a'.*property 'alpha-last'",
                         engine::detail::parse_ulong("alpha-last", "3a"));

    ATF_REQUIRE_THROW_RE(engine::format_error,
                         "value '3 5'.*property 'two-ints'",
                         engine::detail::parse_ulong("two-ints", "3 5"));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_require_files__ok)
ATF_TEST_CASE_BODY(parse_require_files__ok)
{
    const engine::paths_set paths = engine::detail::parse_require_files(
        "unused-name", " /bin/ls /f2 ");
    ATF_REQUIRE_EQ(2, paths.size());
    ATF_REQUIRE_IN(fs::path("/bin/ls"), paths);
    ATF_REQUIRE_IN(fs::path("/f2"), paths);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_require_files__invalid)
ATF_TEST_CASE_BODY(parse_require_files__invalid)
{
    ATF_REQUIRE_THROW_RE(engine::format_error,
                         "Relative path 'data/foo'.*property 'require.files'",
                         engine::detail::parse_require_files(
                             "require.files", "  /bin/ls data/foo "));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_require_progs__ok)
ATF_TEST_CASE_BODY(parse_require_progs__ok)
{
    engine::paths_set paths;

    paths = engine::detail::parse_require_progs("unused-name", " /bin/ls svn ");
    ATF_REQUIRE_EQ(2, paths.size());
    ATF_REQUIRE_IN(fs::path("/bin/ls"), paths);
    ATF_REQUIRE_IN(fs::path("svn"), paths);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_require_progs__invalid)
ATF_TEST_CASE_BODY(parse_require_progs__invalid)
{
    ATF_REQUIRE_THROW_RE(engine::format_error,
                         "Relative path 'bin/svn'.*property 'require.progs'",
                         engine::detail::parse_require_progs(
                             "require.progs", "  /bin/ls bin/svn "));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_require_user__ok)
ATF_TEST_CASE_BODY(parse_require_user__ok)
{
    ATF_REQUIRE_EQ("", engine::detail::parse_require_user("unused-name", ""));
    ATF_REQUIRE_EQ("root", engine::detail::parse_require_user(
        "unused-name", "root"));
    ATF_REQUIRE_EQ("unprivileged", engine::detail::parse_require_user(
        "unused-name", "unprivileged"));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_require_user__invalid)
ATF_TEST_CASE_BODY(parse_require_user__invalid)
{
    ATF_REQUIRE_THROW_RE(engine::format_error,
                         "user ' root'.*property 'require.user'",
                         engine::detail::parse_require_user(
                             "require.user", " root"));
    ATF_REQUIRE_THROW_RE(engine::format_error,
                         "user 'nobody'.*property 'require.user'",
                         engine::detail::parse_require_user(
                             "require.user", "nobody"));
}


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


ATF_TEST_CASE_WITHOUT_HEAD(test_case__public_fields)
ATF_TEST_CASE_BODY(test_case__public_fields)
{
    const engine::test_case_id identifier(fs::path("bin"), "name");
    const std::string description("some text");
    const datetime::delta timeout(1, 2);

    engine::strings_set allowed_architectures;
    allowed_architectures.insert("x86_64");

    engine::strings_set allowed_platforms;
    allowed_platforms.insert("amd64");

    engine::strings_set required_configs;
    required_configs.insert("myvar1");

    engine::paths_set required_files;
    required_files.insert(fs::path("/file1"));

    engine::paths_set required_programs;
    required_programs.insert(fs::path("bin1"));

    engine::properties_map user_metadata;
    user_metadata["X-foo"] = "value1";

    const engine::test_case test_case(identifier,
                                      description,
                                      true,
                                      timeout,
                                      allowed_architectures,
                                      allowed_platforms,
                                      required_configs,
                                      required_files,
                                      required_programs,
                                      "root",
                                      user_metadata);
    ATF_REQUIRE(identifier == test_case.identifier);
    ATF_REQUIRE(description == test_case.description);
    ATF_REQUIRE(test_case.has_cleanup);
    ATF_REQUIRE(timeout == test_case.timeout);
    ATF_REQUIRE(allowed_architectures == test_case.allowed_architectures);
    ATF_REQUIRE(allowed_platforms == test_case.allowed_platforms);
    ATF_REQUIRE(required_configs == test_case.required_configs);
    ATF_REQUIRE(required_files == test_case.required_files);
    ATF_REQUIRE(required_programs == test_case.required_programs);
    ATF_REQUIRE("root" == test_case.required_user);
    ATF_REQUIRE(user_metadata == test_case.user_metadata);
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__from_properties__defaults)
ATF_TEST_CASE_BODY(test_case__from_properties__defaults)
{
    const engine::test_case_id id(fs::path("program"), "test-case");
    const engine::properties_map properties;

    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "test-case"), properties);

    ATF_REQUIRE_EQ(fs::path("program"), test_case.identifier.program);
    ATF_REQUIRE_EQ("test-case", test_case.identifier.name);
    ATF_REQUIRE(!test_case.has_cleanup);
    ATF_REQUIRE(datetime::delta(300, 0) == test_case.timeout);
    ATF_REQUIRE(test_case.allowed_architectures.empty());
    ATF_REQUIRE(test_case.allowed_platforms.empty());
    ATF_REQUIRE(test_case.required_configs.empty());
    ATF_REQUIRE(test_case.required_files.empty());
    ATF_REQUIRE(test_case.required_programs.empty());
    ATF_REQUIRE(test_case.required_user.empty());
    ATF_REQUIRE(test_case.user_metadata.empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__from_properties__override_all)
ATF_TEST_CASE_BODY(test_case__from_properties__override_all)
{
    const engine::test_case_id id(fs::path("program"), "test-case");
    engine::properties_map properties;
    properties["descr"] = "Some text";
    properties["has.cleanup"] = "true";
    properties["require.arch"] = "i386 x86_64";
    properties["require.config"] = "var1 var2 var3";
    properties["require.files"] = "/file1 /dir/file2";
    properties["require.machine"] = "amd64";
    properties["require.progs"] = "/bin/ls svn";
    properties["require.user"] = "root";
    properties["timeout"] = "123";
    properties["X-foo"] = "value1";
    properties["X-bar"] = "value2";
    properties["X-baz-www"] = "value3";

    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "test-case"), properties);

    ATF_REQUIRE_EQ(fs::path("program"), test_case.identifier.program);
    ATF_REQUIRE_EQ("test-case", test_case.identifier.name);
    ATF_REQUIRE(test_case.has_cleanup);
    ATF_REQUIRE(datetime::delta(123, 0) == test_case.timeout);
    ATF_REQUIRE_EQ(2, test_case.allowed_architectures.size());
    ATF_REQUIRE_IN("i386", test_case.allowed_architectures);
    ATF_REQUIRE_IN("x86_64", test_case.allowed_architectures);
    ATF_REQUIRE_EQ(1, test_case.allowed_platforms.size());
    ATF_REQUIRE_IN("amd64", test_case.allowed_platforms);
    ATF_REQUIRE_EQ(3, test_case.required_configs.size());
    ATF_REQUIRE_IN("var1", test_case.required_configs);
    ATF_REQUIRE_IN("var2", test_case.required_configs);
    ATF_REQUIRE_IN("var3", test_case.required_configs);
    ATF_REQUIRE_EQ(2, test_case.required_files.size());
    ATF_REQUIRE_IN(fs::path("/file1"), test_case.required_files);
    ATF_REQUIRE_IN(fs::path("/dir/file2"), test_case.required_files);
    ATF_REQUIRE_EQ(2, test_case.required_programs.size());
    ATF_REQUIRE_IN(fs::path("/bin/ls"), test_case.required_programs);
    ATF_REQUIRE_IN(fs::path("svn"), test_case.required_programs);
    ATF_REQUIRE_EQ("root", test_case.required_user);
    ATF_REQUIRE_EQ(3, test_case.user_metadata.size());
    ATF_REQUIRE_EQ("value1", test_case.user_metadata.find("X-foo")->second);
    ATF_REQUIRE_EQ("value2", test_case.user_metadata.find("X-bar")->second);
    ATF_REQUIRE_EQ("value3", test_case.user_metadata.find("X-baz-www")->second);
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__from_properties__unknown)
ATF_TEST_CASE_BODY(test_case__from_properties__unknown)
{
    const engine::test_case_id id(fs::path("program"), "test-case");
    engine::properties_map properties;
    properties["foobar"] = "Some text";

    ATF_REQUIRE_THROW_RE(engine::format_error, "Unknown.*property.*'foobar'",
        engine::test_case::from_properties(engine::test_case_id(
            fs::path("program"), "test-case"), properties));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__all_properties__none)
ATF_TEST_CASE_BODY(test_case__all_properties__none)
{
    const engine::test_case_id id(fs::path("program"), "test-case");
    engine::properties_map in_properties;
    engine::properties_map exp_properties;

    ATF_REQUIRE(exp_properties == engine::test_case::from_properties(
        id, in_properties).all_properties());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__all_properties__only_user)
ATF_TEST_CASE_BODY(test_case__all_properties__only_user)
{
    const engine::test_case_id id(fs::path("program"), "test-case");

    engine::properties_map in_properties;
    in_properties["X-foo"] = "bar";
    in_properties["X-another-var"] = "This is a string";

    engine::properties_map exp_properties = in_properties;

    ATF_REQUIRE(exp_properties == engine::test_case::from_properties(
        id, in_properties).all_properties());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__all_properties__all)
ATF_TEST_CASE_BODY(test_case__all_properties__all)
{
    const engine::test_case_id id(fs::path("program"), "test-case");

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

    ATF_REQUIRE(exp_properties == engine::test_case::from_properties(
        id, in_properties).all_properties());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_case__operator_eq);
ATF_TEST_CASE_BODY(test_case__operator_eq)
{
    const engine::test_case original = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"),
        engine::properties_map());
    ATF_REQUIRE(original == original);

    const engine::test_case change_id = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program2"), "name"),
        engine::properties_map());
    ATF_REQUIRE(!(original == change_id));

    engine::properties_map overrides;
    overrides["descr"] = "Some text";
    overrides["has.cleanup"] = "true";
    overrides["require.arch"] = "i386 x86_64";
    overrides["require.config"] = "var1 var2 var3";
    overrides["require.files"] = "/file1 /file2";
    overrides["require.machine"] = "amd64";
    overrides["require.progs"] = "/bin/ls svn";
    overrides["require.user"] = "root";
    overrides["timeout"] = "123";
    overrides["X-foo"] = "value1";

    for (engine::properties_map::const_iterator iter = overrides.begin();
         iter != overrides.end(); iter++) {
        engine::properties_map properties;
        properties[(*iter).first] = (*iter).second;
        const engine::test_case modified = engine::test_case::from_properties(
            original.identifier, properties);
        ATF_REQUIRE(modified == modified);
        ATF_REQUIRE(!(original == modified));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(check_requirements__none);
ATF_TEST_CASE_BODY(check_requirements__none)
{
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"),
        engine::properties_map());
    ATF_REQUIRE(engine::check_requirements(test_case, mock_config,
                                           "test-suite").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_requirements__required_architectures__one_ok);
ATF_TEST_CASE_BODY(check_requirements__required_architectures__one_ok)
{
    engine::properties_map metadata;
    metadata["require.arch"] = "x86_64";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    user_files::config config = mock_config;
    config.architecture = "x86_64";
    config.platform = "";
    ATF_REQUIRE(engine::check_requirements(test_case, config, "").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(
    check_requirements__required_architectures__one_fail);
ATF_TEST_CASE_BODY(check_requirements__required_architectures__one_fail)
{
    engine::properties_map metadata;
    metadata["require.arch"] = "x86_64";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    user_files::config config = mock_config;
    config.architecture = "i386";
    config.platform = "";
    ATF_REQUIRE_MATCH("Current architecture 'i386' not supported",
                      engine::check_requirements(test_case, config, ""));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_requirements__required_architectures__many_ok);
ATF_TEST_CASE_BODY(check_requirements__required_architectures__many_ok)
{
    engine::properties_map metadata;
    metadata["require.arch"] = "x86_64 i386 powerpc";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    user_files::config config = mock_config;
    config.architecture = "i386";
    config.platform = "";
    ATF_REQUIRE(engine::check_requirements(test_case, config, "").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(
    check_requirements__required_architectures__many_fail);
ATF_TEST_CASE_BODY(check_requirements__required_architectures__many_fail)
{
    engine::properties_map metadata;
    metadata["require.arch"] = "x86_64 i386 powerpc";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    user_files::config config = mock_config;
    config.architecture = "arm";
    config.platform = "";
    ATF_REQUIRE_MATCH("Current architecture 'arm' not supported",
                      engine::check_requirements(test_case, config, ""));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_requirements__required_platforms__one_ok);
ATF_TEST_CASE_BODY(check_requirements__required_platforms__one_ok)
{
    engine::properties_map metadata;
    metadata["require.machine"] = "amd64";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    user_files::config config = mock_config;
    config.architecture = "";
    config.platform = "amd64";
    ATF_REQUIRE(engine::check_requirements(test_case, config, "").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_requirements__required_platforms__one_fail);
ATF_TEST_CASE_BODY(check_requirements__required_platforms__one_fail)
{
    engine::properties_map metadata;
    metadata["require.machine"] = "amd64";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    user_files::config config = mock_config;
    config.architecture = "";
    config.platform = "i386";
    ATF_REQUIRE_MATCH("Current platform 'i386' not supported",
                      engine::check_requirements(test_case, config, ""));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_requirements__required_platforms__many_ok);
ATF_TEST_CASE_BODY(check_requirements__required_platforms__many_ok)
{
    engine::properties_map metadata;
    metadata["require.machine"] = "amd64 i386 macppc";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    user_files::config config = mock_config;
    config.architecture = "";
    config.platform = "i386";
    ATF_REQUIRE(engine::check_requirements(test_case, config, "").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_requirements__required_platforms__many_fail);
ATF_TEST_CASE_BODY(check_requirements__required_platforms__many_fail)
{
    engine::properties_map metadata;
    metadata["require.machine"] = "amd64 i386 macppc";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    user_files::config config = mock_config;
    config.architecture = "";
    config.platform = "shark";
    ATF_REQUIRE_MATCH("Current platform 'shark' not supported",
                      engine::check_requirements(test_case, config, ""));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_requirements__required_configs__one_ok);
ATF_TEST_CASE_BODY(check_requirements__required_configs__one_ok)
{
    engine::properties_map metadata;
    metadata["require.config"] = "my-var";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    user_files::config config = mock_config;
    config.test_suites["suite"]["aaa"] = "value1";
    config.test_suites["suite"]["my-var"] = "value2";
    config.test_suites["suite"]["zzz"] = "value3";
    ATF_REQUIRE(engine::check_requirements(test_case, config, "suite").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_requirements__required_configs__one_fail);
ATF_TEST_CASE_BODY(check_requirements__required_configs__one_fail)
{
    engine::properties_map metadata;
    metadata["require.config"] = "unprivileged-user";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    user_files::config config = mock_config;
    config.test_suites["suite"]["aaa"] = "value1";
    config.test_suites["suite"]["myvar"] = "value2";
    config.test_suites["suite"]["zzz"] = "value3";
    ATF_REQUIRE_MATCH("Required configuration property 'unprivileged-user' not "
                      "defined",
                      engine::check_requirements(test_case, config, "suite"));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_requirements__required_configs__many_ok);
ATF_TEST_CASE_BODY(check_requirements__required_configs__many_ok)
{
    engine::properties_map metadata;
    metadata["require.config"] = "foo bar baz";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    user_files::config config = mock_config;
    config.test_suites["suite"]["aaa"] = "value1";
    config.test_suites["suite"]["foo"] = "value2";
    config.test_suites["suite"]["bar"] = "value3";
    config.test_suites["suite"]["baz"] = "value4";
    config.test_suites["suite"]["zzz"] = "value5";
    ATF_REQUIRE(engine::check_requirements(test_case, config, "suite").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_requirements__required_configs__many_fail);
ATF_TEST_CASE_BODY(check_requirements__required_configs__many_fail)
{
    engine::properties_map metadata;
    metadata["require.config"] = "foo bar baz";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    user_files::config config = mock_config;
    config.test_suites["suite"]["aaa"] = "value1";
    config.test_suites["suite"]["foo"] = "value2";
    config.test_suites["suite"]["zzz"] = "value3";
    ATF_REQUIRE_MATCH("Required configuration property 'bar' not defined",
                      engine::check_requirements(test_case, config, "config"));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_requirements__required_configs__special);
ATF_TEST_CASE_BODY(check_requirements__required_configs__special)
{
    engine::properties_map metadata;
    metadata["require.config"] = "unprivileged-user";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    user_files::config config = mock_config;
    config.unprivileged_user = passwd::user("foo", 1, 2);
    ATF_REQUIRE(engine::check_requirements(test_case, config, "").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_requirements__required_user__root__ok);
ATF_TEST_CASE_BODY(check_requirements__required_user__root__ok)
{
    engine::properties_map metadata;
    metadata["require.user"] = "root";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    passwd::set_current_user_for_testing(passwd::user("", 0, 1));
    ATF_REQUIRE(engine::check_requirements(test_case, mock_config, "").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_requirements__required_user__root__fail);
ATF_TEST_CASE_BODY(check_requirements__required_user__root__fail)
{
    engine::properties_map metadata;
    metadata["require.user"] = "root";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    passwd::set_current_user_for_testing(passwd::user("", 123, 1));
    ATF_REQUIRE_MATCH("Requires root privileges",
                      engine::check_requirements(test_case, mock_config, ""));
}


ATF_TEST_CASE_WITHOUT_HEAD(
    check_requirements__required_user__unprivileged__same);
ATF_TEST_CASE_BODY(check_requirements__required_user__unprivileged__same)
{
    engine::properties_map metadata;
    metadata["require.user"] = "unprivileged";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    user_files::config config = mock_config;
    config.unprivileged_user = utils::none;

    passwd::set_current_user_for_testing(passwd::user("", 123, 1));
    ATF_REQUIRE(engine::check_requirements(test_case, config, "").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_requirements__required_user__unprivileged__ok);
ATF_TEST_CASE_BODY(check_requirements__required_user__unprivileged__ok)
{
    engine::properties_map metadata;
    metadata["require.user"] = "unprivileged";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    user_files::config config = mock_config;
    config.unprivileged_user = passwd::user("", 123, 1);

    passwd::set_current_user_for_testing(passwd::user("", 0, 1));
    ATF_REQUIRE(engine::check_requirements(test_case, config, "").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(
    check_requirements__required_user__unprivileged__fail);
ATF_TEST_CASE_BODY(check_requirements__required_user__unprivileged__fail)
{
    engine::properties_map metadata;
    metadata["require.user"] = "unprivileged";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    user_files::config config = mock_config;
    config.unprivileged_user = utils::none;

    passwd::set_current_user_for_testing(passwd::user("", 0, 1));
    ATF_REQUIRE_MATCH("Requires.*unprivileged.*unprivileged-user",
                      engine::check_requirements(test_case, config, ""));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_requirements__required_files__ok);
ATF_TEST_CASE_BODY(check_requirements__required_files__ok)
{
    utils::create_file(fs::path("test-file"));

    engine::properties_map metadata;
    metadata["require.files"] = (fs::current_path() / "test-file").str();
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    ATF_REQUIRE(engine::check_requirements(test_case, mock_config, "").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_requirements__required_files__fail);
ATF_TEST_CASE_BODY(check_requirements__required_files__fail)
{
    engine::properties_map metadata;
    metadata["require.files"] = "/non-existent/file";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    ATF_REQUIRE_MATCH("'/non-existent/file' not found$",
                      engine::check_requirements(test_case, mock_config, ""));
}


ATF_TEST_CASE(check_requirements__required_programs__ok);
ATF_TEST_CASE_HEAD(check_requirements__required_programs__ok)
{
    set_md_var("require.progs", "/bin/ls /bin/mv");
}
ATF_TEST_CASE_BODY(check_requirements__required_programs__ok)
{
    fs::mkdir(fs::path("bin"), 0755);
    utils::create_file(fs::path("bin/foo"));
    utils::setenv("PATH", (fs::current_path() / "bin").str());

    engine::properties_map metadata;
    metadata["require.progs"] = "/bin/ls foo /bin/mv";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    ATF_REQUIRE(engine::check_requirements(test_case, mock_config, "").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(
    check_requirements__required_programs__fail_absolute);
ATF_TEST_CASE_BODY(check_requirements__required_programs__fail_absolute)
{
    engine::properties_map metadata;
    metadata["require.progs"] = "/non-existent/program";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    ATF_REQUIRE_MATCH("'/non-existent/program' not found$",
                      engine::check_requirements(test_case, mock_config, ""));
}


ATF_TEST_CASE_WITHOUT_HEAD(
    check_requirements__required_programs__fail_relative);
ATF_TEST_CASE_BODY(check_requirements__required_programs__fail_relative)
{
    fs::mkdir(fs::path("bin"), 0755);
    utils::create_file(fs::path("bin/foo"));
    utils::setenv("PATH", (fs::current_path() / "bin").str());

    engine::properties_map metadata;
    metadata["require.progs"] = "foo bar";
    const engine::test_case test_case = engine::test_case::from_properties(
        engine::test_case_id(fs::path("program"), "name"), metadata);

    ATF_REQUIRE_MATCH("'bar' not found in PATH$",
                      engine::check_requirements(test_case, mock_config, ""));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, parse_bool__true);
    ATF_ADD_TEST_CASE(tcs, parse_bool__false);
    ATF_ADD_TEST_CASE(tcs, parse_bool__invalid);

    ATF_ADD_TEST_CASE(tcs, parse_list__empty);
    ATF_ADD_TEST_CASE(tcs, parse_list__one_word);
    ATF_ADD_TEST_CASE(tcs, parse_list__many_words);

    ATF_ADD_TEST_CASE(tcs, parse_require_files__ok);
    ATF_ADD_TEST_CASE(tcs, parse_require_files__invalid);

    ATF_ADD_TEST_CASE(tcs, parse_require_progs__ok);
    ATF_ADD_TEST_CASE(tcs, parse_require_progs__invalid);

    ATF_ADD_TEST_CASE(tcs, parse_require_user__ok);
    ATF_ADD_TEST_CASE(tcs, parse_require_user__invalid);

    ATF_ADD_TEST_CASE(tcs, parse_ulong__ok);
    ATF_ADD_TEST_CASE(tcs, parse_ulong__empty);
    ATF_ADD_TEST_CASE(tcs, parse_ulong__invalid);

    ATF_ADD_TEST_CASE(tcs, test_case_id__public_fields);
    ATF_ADD_TEST_CASE(tcs, test_case_id__str);
    ATF_ADD_TEST_CASE(tcs, test_case_id__operator_lt);
    ATF_ADD_TEST_CASE(tcs, test_case_id__operator_eq);
    ATF_ADD_TEST_CASE(tcs, test_case_id__use_as_key);

    ATF_ADD_TEST_CASE(tcs, test_case__public_fields);
    ATF_ADD_TEST_CASE(tcs, test_case__from_properties__defaults);
    ATF_ADD_TEST_CASE(tcs, test_case__from_properties__override_all);
    ATF_ADD_TEST_CASE(tcs, test_case__from_properties__unknown);
    ATF_ADD_TEST_CASE(tcs, test_case__all_properties__none);
    ATF_ADD_TEST_CASE(tcs, test_case__all_properties__only_user);
    ATF_ADD_TEST_CASE(tcs, test_case__all_properties__all);
    ATF_ADD_TEST_CASE(tcs, test_case__operator_eq);

    ATF_ADD_TEST_CASE(tcs, check_requirements__none);
    ATF_ADD_TEST_CASE(tcs, check_requirements__required_architectures__one_ok);
    ATF_ADD_TEST_CASE(tcs,
                      check_requirements__required_architectures__one_fail);
    ATF_ADD_TEST_CASE(tcs, check_requirements__required_architectures__many_ok);
    ATF_ADD_TEST_CASE(tcs,
                      check_requirements__required_architectures__many_fail);
    ATF_ADD_TEST_CASE(tcs, check_requirements__required_platforms__one_ok);
    ATF_ADD_TEST_CASE(tcs, check_requirements__required_platforms__one_fail);
    ATF_ADD_TEST_CASE(tcs, check_requirements__required_platforms__many_ok);
    ATF_ADD_TEST_CASE(tcs, check_requirements__required_platforms__many_fail);
    ATF_ADD_TEST_CASE(tcs, check_requirements__required_configs__one_ok);
    ATF_ADD_TEST_CASE(tcs, check_requirements__required_configs__one_fail);
    ATF_ADD_TEST_CASE(tcs, check_requirements__required_configs__many_ok);
    ATF_ADD_TEST_CASE(tcs, check_requirements__required_configs__many_fail);
    ATF_ADD_TEST_CASE(tcs, check_requirements__required_configs__special);
    ATF_ADD_TEST_CASE(tcs, check_requirements__required_user__root__ok);
    ATF_ADD_TEST_CASE(tcs, check_requirements__required_user__root__fail);
    ATF_ADD_TEST_CASE(tcs,
                      check_requirements__required_user__unprivileged__same);
    ATF_ADD_TEST_CASE(tcs, check_requirements__required_user__unprivileged__ok);
    ATF_ADD_TEST_CASE(tcs,
                      check_requirements__required_user__unprivileged__fail);
    ATF_ADD_TEST_CASE(tcs, check_requirements__required_files__ok);
    ATF_ADD_TEST_CASE(tcs, check_requirements__required_files__fail);
    ATF_ADD_TEST_CASE(tcs, check_requirements__required_programs__ok);
    ATF_ADD_TEST_CASE(tcs,
                      check_requirements__required_programs__fail_absolute);
    ATF_ADD_TEST_CASE(tcs,
                      check_requirements__required_programs__fail_relative);
}
