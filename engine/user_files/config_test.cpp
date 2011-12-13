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

#if defined(HAVE_CONFIG_H)
#   include "config.h"
#endif

#include <fstream>
#include <stdexcept>
#include <vector>

#include <atf-c++.hpp>
#include <lutok/operations.hpp>
#include <lutok/state.ipp>
#include <lutok/test_utils.hpp>

#include "engine/user_files/config.hpp"
#include "engine/user_files/exceptions.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/passwd.hpp"

namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace user_files = engine::user_files;

using utils::none;
using utils::optional;


namespace {


/// Replaces the system user database with a fake one for testing purposes.
static void
set_mock_users(void)
{
    std::vector< passwd::user > users;
    users.push_back(passwd::user("user1", 100, 150));
    users.push_back(passwd::user("user2", 200, 250));
    passwd::set_mock_users_for_testing(users);
}


/// Checks that the default values of a config object match our expectations.
///
/// This fails the test case if any field of the input config object is not
/// what we expect.
///
/// \param config The configuration to validate.
static void
validate_defaults(const user_files::config& config)
{
    ATF_REQUIRE_EQ(KYUA_ARCHITECTURE, config.architecture);
    ATF_REQUIRE_EQ(KYUA_PLATFORM, config.platform);
    ATF_REQUIRE(!config.unprivileged_user);
    ATF_REQUIRE(config.test_suites.empty());
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(get_properties__none);
ATF_TEST_CASE_BODY(get_properties__none)
{
    lutok::state state;
    stack_balance_checker checker(state);

    state.new_table();

    const user_files::properties_map properties =
        user_files::detail::get_properties(state, "the-name");
    ATF_REQUIRE(properties.empty());

    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(get_properties__some);
ATF_TEST_CASE_BODY(get_properties__some)
{
    lutok::state state;
    stack_balance_checker checker(state);

    lutok::do_string(state, "return {empty_string='', a_string='foo bar', "
                     "real_int=3, int_as_string='5', bool_true=true, "
                     "bool_false=false}", 1);
    
    const user_files::properties_map properties =
        user_files::detail::get_properties(state, "the-name");
    ATF_REQUIRE_EQ(6, properties.size());

    user_files::properties_map::const_iterator iter;

    iter = properties.find("empty_string");
    ATF_REQUIRE(iter != properties.end());
    ATF_REQUIRE_EQ("", (*iter).second);

    iter = properties.find("a_string");
    ATF_REQUIRE(iter != properties.end());
    ATF_REQUIRE_EQ("foo bar", (*iter).second);

    iter = properties.find("real_int");
    ATF_REQUIRE(iter != properties.end());
    ATF_REQUIRE_EQ("3", (*iter).second);

    iter = properties.find("int_as_string");
    ATF_REQUIRE(iter != properties.end());
    ATF_REQUIRE_EQ("5", (*iter).second);

    iter = properties.find("bool_true");
    ATF_REQUIRE(iter != properties.end());
    ATF_REQUIRE_EQ("true", (*iter).second);

    iter = properties.find("bool_false");
    ATF_REQUIRE(iter != properties.end());
    ATF_REQUIRE_EQ("false", (*iter).second);

    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(get_properties__bad_key);
ATF_TEST_CASE_BODY(get_properties__bad_key)
{
    lutok::state state;
    stack_balance_checker checker(state);

    lutok::do_string(state, "t = {}; t['a']=5; t[{}]=3; return t", 1);

    ATF_REQUIRE_THROW_RE(
        std::runtime_error, "non-string property name.*'the-name'",
        user_files::detail::get_properties(state, "the-name"));

    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(get_properties__bad_value);
ATF_TEST_CASE_BODY(get_properties__bad_value)
{
    lutok::state state;
    stack_balance_checker checker(state);

    lutok::do_string(state, "return {a=5, b={}}", 1);

    ATF_REQUIRE_THROW_RE(
        std::runtime_error, "Invalid value.*property 'b'.*'foo'",
        user_files::detail::get_properties(state, "foo"));

    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(get_string_var__nil);
ATF_TEST_CASE_BODY(get_string_var__nil)
{
    lutok::state state;
    stack_balance_checker checker(state);
    ATF_REQUIRE_EQ("default-value", user_files::detail::get_string_var(
        state, "undefined", "default-value"));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_string_var__ok);
ATF_TEST_CASE_BODY(get_string_var__ok)
{
    lutok::state state;
    lutok::do_string(state, "myvar = 'foo bar baz'");

    stack_balance_checker checker(state);
    ATF_REQUIRE_EQ("foo bar baz", user_files::detail::get_string_var(
        state, "myvar", "default-value"));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_string_var__invalid);
ATF_TEST_CASE_BODY(get_string_var__invalid)
{
    lutok::state state;
    lutok::do_string(state, "myvar = {}");

    stack_balance_checker checker(state);
    ATF_REQUIRE_THROW_RE(
        std::runtime_error, "Invalid type.*'myvar'",
        user_files::detail::get_string_var(state, "myvar", "default-value"));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_suites__none);
ATF_TEST_CASE_BODY(get_test_suites__none)
{
    lutok::state state;
    stack_balance_checker checker(state);

    lutok::do_string(state, "ts = {}");

    const user_files::test_suites_map test_suites =
        user_files::detail::get_test_suites(state, "ts");
    ATF_REQUIRE(test_suites.empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_suites__some);
ATF_TEST_CASE_BODY(get_test_suites__some)
{
    lutok::state state;
    stack_balance_checker checker(state);

    lutok::do_string(state, "ts = {foo={a=3, b='hello', c=true}, "
                     "bar={}, baz={prop='value'}}");

    const user_files::test_suites_map test_suites =
        user_files::detail::get_test_suites(state, "ts");
    ATF_REQUIRE_EQ(2, test_suites.size());

    user_files::test_suites_map::const_iterator iter;

    iter = test_suites.find("foo");
    ATF_REQUIRE(iter != test_suites.end());
    {
        const user_files::properties_map& properties = (*iter).second;
        ATF_REQUIRE_EQ(3, properties.size());

        user_files::properties_map::const_iterator iter2;

        iter2 = properties.find("a");
        ATF_REQUIRE(iter2 != properties.end());
        ATF_REQUIRE_EQ("3", (*iter2).second);

        iter2 = properties.find("b");
        ATF_REQUIRE(iter2 != properties.end());
        ATF_REQUIRE_EQ("hello", (*iter2).second);

        iter2 = properties.find("c");
        ATF_REQUIRE(iter2 != properties.end());
        ATF_REQUIRE_EQ("true", (*iter2).second);
    }

    iter = test_suites.find("baz");
    ATF_REQUIRE(iter != test_suites.end());
    {
        const user_files::properties_map& properties = (*iter).second;
        ATF_REQUIRE_EQ(1, properties.size());

        user_files::properties_map::const_iterator iter2;

        iter2 = properties.find("prop");
        ATF_REQUIRE(iter2 != properties.end());
        ATF_REQUIRE_EQ("value", (*iter2).second);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_suites__invalid);
ATF_TEST_CASE_BODY(get_test_suites__invalid)
{
    lutok::state state;
    stack_balance_checker checker(state);

    lutok::do_string(state, "ts = 'I should be a table'");

    ATF_REQUIRE_THROW_RE(std::runtime_error, "'ts'.*not a table",
                         user_files::detail::get_test_suites(state, "ts"));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_suites__bad_key);
ATF_TEST_CASE_BODY(get_test_suites__bad_key)
{
    lutok::state state;
    stack_balance_checker checker(state);

    lutok::do_string(state, "ts = {}; ts['a'] = {}; ts[{}] = 'the key is bad'");

    ATF_REQUIRE_THROW_RE(std::runtime_error, "non-string.*suite name.*'ts'",
                         user_files::detail::get_test_suites(state, "ts"));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_suites__bad_value);
ATF_TEST_CASE_BODY(get_test_suites__bad_value)
{
    lutok::state state;
    stack_balance_checker checker(state);

    lutok::do_string(state, "ts = {}; ts['a'] = {}; ts['b'] = 'bad value'");

    ATF_REQUIRE_THROW_RE(std::runtime_error, "non-table properties.*'b'",
                         user_files::detail::get_test_suites(state, "ts"));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_user_var__nil);
ATF_TEST_CASE_BODY(get_user_var__nil)
{
    set_mock_users();

    lutok::state state;
    stack_balance_checker checker(state);
    ATF_REQUIRE(!user_files::detail::get_user_var(state, "undefined"));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_user_var__uid_ok);
ATF_TEST_CASE_BODY(get_user_var__uid_ok)
{
    set_mock_users();

    lutok::state state;
    lutok::do_string(state, "myvar = 100");

    stack_balance_checker checker(state);
    const optional< passwd::user > user = user_files::detail::get_user_var(
        state, "myvar");
    ATF_REQUIRE(user);
    ATF_REQUIRE_EQ("user1", user.get().name);
    ATF_REQUIRE_EQ(100, user.get().uid);
}


ATF_TEST_CASE_WITHOUT_HEAD(get_user_var__uid_error);
ATF_TEST_CASE_BODY(get_user_var__uid_error)
{
    set_mock_users();

    lutok::state state;
    lutok::do_string(state, "myvar = '150'");

    stack_balance_checker checker(state);
    ATF_REQUIRE_THROW_RE(
        std::runtime_error, "Cannot find user.*UID 150.*'myvar'",
        user_files::detail::get_user_var(state, "myvar"));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_user_var__name_ok);
ATF_TEST_CASE_BODY(get_user_var__name_ok)
{
    set_mock_users();

    lutok::state state;
    lutok::do_string(state, "myvar = 'user2'");

    stack_balance_checker checker(state);
    const optional< passwd::user > user = user_files::detail::get_user_var(
        state, "myvar");
    ATF_REQUIRE(user);
    ATF_REQUIRE_EQ("user2", user.get().name);
    ATF_REQUIRE_EQ(200, user.get().uid);
}


ATF_TEST_CASE_WITHOUT_HEAD(get_user_var__name_error);
ATF_TEST_CASE_BODY(get_user_var__name_error)
{
    set_mock_users();

    lutok::state state;
    lutok::do_string(state, "myvar = 'root'");

    stack_balance_checker checker(state);
    ATF_REQUIRE_THROW_RE(
        std::runtime_error, "Cannot find user.*name 'root'.*'myvar'",
        user_files::detail::get_user_var(state, "myvar"));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_user_var__invalid);
ATF_TEST_CASE_BODY(get_user_var__invalid)
{
    set_mock_users();

    lutok::state state;
    lutok::do_string(state, "myvar = {}");

    stack_balance_checker checker(state);
    ATF_REQUIRE_THROW_RE(std::runtime_error, "Invalid type.*'myvar'",
                         user_files::detail::get_user_var(state, "myvar"));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_user_override__uid_ok);
ATF_TEST_CASE_BODY(get_user_override__uid_ok)
{
    set_mock_users();

    const optional< passwd::user > user = user_files::detail::get_user_override(
        "myvar", "100");
    ATF_REQUIRE(user);
    ATF_REQUIRE_EQ("user1", user.get().name);
    ATF_REQUIRE_EQ(100, user.get().uid);
}


ATF_TEST_CASE_WITHOUT_HEAD(get_user_override__uid_error);
ATF_TEST_CASE_BODY(get_user_override__uid_error)
{
    set_mock_users();

    ATF_REQUIRE_THROW_RE(
        std::runtime_error, "Cannot find user.*UID 150.*'myvar=150'",
        user_files::detail::get_user_override("myvar", "150"));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_user_override__name_ok);
ATF_TEST_CASE_BODY(get_user_override__name_ok)
{
    set_mock_users();

    const optional< passwd::user > user = user_files::detail::get_user_override(
        "myvar", "user2");
    ATF_REQUIRE(user);
    ATF_REQUIRE_EQ("user2", user.get().name);
    ATF_REQUIRE_EQ(200, user.get().uid);
}


ATF_TEST_CASE_WITHOUT_HEAD(get_user_override__name_error);
ATF_TEST_CASE_BODY(get_user_override__name_error)
{
    set_mock_users();

    ATF_REQUIRE_THROW_RE(
        std::runtime_error, "Cannot find user.*name 'root'.*'myvar=root'",
        user_files::detail::get_user_override("myvar", "root"));
}


ATF_TEST_CASE_WITHOUT_HEAD(config__defaults);
ATF_TEST_CASE_BODY(config__defaults)
{
    const user_files::config config = user_files::config::defaults();
    validate_defaults(config);
}


ATF_TEST_CASE_WITHOUT_HEAD(config__load__defaults);
ATF_TEST_CASE_BODY(config__load__defaults)
{
    {
        std::ofstream file("config");
        file << "syntax('config', 1)\n";
        file.close();
    }

    const user_files::config config = user_files::config::load(
        fs::path("config"));
    validate_defaults(config);
}


ATF_TEST_CASE_WITHOUT_HEAD(config__load__overrides);
ATF_TEST_CASE_BODY(config__load__overrides)
{
    set_mock_users();

    {
        std::ofstream file("config");
        file << "syntax('config', 1)\n";
        file << "architecture = 'test-architecture'\n";
        file << "platform = 'test-platform'\n";
        file << "unprivileged_user = 'user2'\n";
        file << "test_suites.mysuite.myvar = 'myvalue'\n";
        file.close();
    }

    const user_files::config config = user_files::config::load(
        fs::path("config"));

    ATF_REQUIRE_EQ("test-architecture", config.architecture);
    ATF_REQUIRE_EQ("test-platform", config.platform);

    ATF_REQUIRE(config.unprivileged_user);
    ATF_REQUIRE_EQ("user2", config.unprivileged_user.get().name);
    ATF_REQUIRE_EQ(200, config.unprivileged_user.get().uid);

    ATF_REQUIRE_EQ(1, config.test_suites.size());
    const user_files::test_suites_map::const_iterator& iter =
        config.test_suites.find("mysuite");
    ATF_REQUIRE(iter != config.test_suites.end());
    const user_files::properties_map& properties = (*iter).second;
    ATF_REQUIRE_EQ(1, properties.size());
    const user_files::properties_map::const_iterator& iter2 =
        properties.find("myvar");
    ATF_REQUIRE(iter2 != properties.end());
    ATF_REQUIRE_EQ("myvalue", (*iter2).second);
}


ATF_TEST_CASE_WITHOUT_HEAD(config__load__lua_error);
ATF_TEST_CASE_BODY(config__load__lua_error)
{
    std::ofstream file("config");
    file << "this syntax is invalid\n";
    file.close();

    ATF_REQUIRE_THROW(user_files::load_error, user_files::config::load(
        fs::path("config")));
}


ATF_TEST_CASE_WITHOUT_HEAD(config__load__bad_syntax__format);
ATF_TEST_CASE_BODY(config__load__bad_syntax__format)
{
    std::ofstream file("config");
    file << "syntax('config', 1)\n";
    file << "init.get_syntax().format = 'foo'\n";
    file.close();

    ATF_REQUIRE_THROW_RE(user_files::load_error, "Unexpected file format 'foo'",
                         user_files::config::load(fs::path("config")));
}


ATF_TEST_CASE_WITHOUT_HEAD(config__load__bad_syntax__version);
ATF_TEST_CASE_BODY(config__load__bad_syntax__version)
{
    std::ofstream file("config");
    file << "syntax('config', 1)\n";
    file << "init.get_syntax().version = 123\n";
    file.close();

    ATF_REQUIRE_THROW_RE(user_files::load_error, "Unexpected file version '123'",
                         user_files::config::load(fs::path("config")));
}


ATF_TEST_CASE_WITHOUT_HEAD(config__load__missing_file);
ATF_TEST_CASE_BODY(config__load__missing_file)
{
    ATF_REQUIRE_THROW_RE(user_files::load_error, "Load of 'missing' failed",
                         user_files::config::load(fs::path("missing")));
}


ATF_TEST_CASE_WITHOUT_HEAD(config__apply_overrides__none);
ATF_TEST_CASE_BODY(config__apply_overrides__none)
{
    user_files::properties_map props;
    props["key1"] = "value1";

    user_files::config config = user_files::config::defaults();
    config.test_suites["suite1"] = props;

    const std::vector< user_files::override_pair > overrides;
    ATF_REQUIRE(config == config.apply_overrides(overrides));
}


ATF_TEST_CASE_WITHOUT_HEAD(config__apply_overrides__architecture);
ATF_TEST_CASE_BODY(config__apply_overrides__architecture)
{
    user_files::properties_map props;
    props["key1"] = "value1";

    user_files::config config = user_files::config::defaults();
    config.architecture = "foo";
    config.test_suites["suite1"] = props;

    std::vector< user_files::override_pair > overrides;
    overrides.push_back(user_files::override_pair("architecture", "bar"));

    const user_files::config new_config = config.apply_overrides(overrides);
    ATF_REQUIRE(config != new_config);
    config.architecture = "bar";
    ATF_REQUIRE(config == new_config);
}


ATF_TEST_CASE_WITHOUT_HEAD(config__apply_overrides__platform);
ATF_TEST_CASE_BODY(config__apply_overrides__platform)
{
    user_files::properties_map props;
    props["key1"] = "value1";

    user_files::config config = user_files::config::defaults();
    config.platform = "foo";
    config.test_suites["suite1"] = props;

    std::vector< user_files::override_pair > overrides;
    overrides.push_back(user_files::override_pair("platform", "bar"));

    const user_files::config new_config = config.apply_overrides(overrides);
    ATF_REQUIRE(config != new_config);
    config.platform = "bar";
    ATF_REQUIRE(config == new_config);
}


ATF_TEST_CASE_WITHOUT_HEAD(config__apply_overrides__unprivileged_user__ok);
ATF_TEST_CASE_BODY(config__apply_overrides__unprivileged_user__ok)
{
    user_files::properties_map props;
    props["key1"] = "value1";

    user_files::config config = user_files::config::defaults();
    config.test_suites["suite1"] = props;

    std::vector< user_files::override_pair > overrides;
    overrides.push_back(user_files::override_pair("unprivileged_user",
                                                  "user1"));

    set_mock_users();
    const user_files::config new_config = config.apply_overrides(overrides);
    ATF_REQUIRE(config != new_config);
    config.unprivileged_user = passwd::user("user1", 100, 150);
    ATF_REQUIRE(config == new_config);
}


ATF_TEST_CASE_WITHOUT_HEAD(config__apply_overrides__unprivileged_user__invalid);
ATF_TEST_CASE_BODY(config__apply_overrides__unprivileged_user__invalid)
{
    const user_files::config config = user_files::config::defaults();

    std::vector< user_files::override_pair > overrides;
    overrides.push_back(user_files::override_pair("unprivileged_user",
                                                  "root"));

    set_mock_users();
    ATF_REQUIRE_THROW_RE(std::runtime_error,
                         "Cannot find user.*'root'.*'unprivileged_user=root'",
                         config.apply_overrides(overrides));
}


ATF_TEST_CASE_WITHOUT_HEAD(config__apply_overrides__test_suite__ok);
ATF_TEST_CASE_BODY(config__apply_overrides__test_suite__ok)
{
    user_files::properties_map props;
    props["key1"] = "value1";
    props["key2"] = "value8";

    user_files::config config = user_files::config::defaults();
    config.test_suites["suite1"] = props;

    std::vector< user_files::override_pair > overrides;
    overrides.push_back(user_files::override_pair("suite1.key1", "value2"));
    overrides.push_back(user_files::override_pair("suite3.key1", "value5"));

    const user_files::config new_config = config.apply_overrides(overrides);
    ATF_REQUIRE(config != new_config);
    config.test_suites["suite1"]["key1"] = "value2";
    config.test_suites["suite3"]["key1"] = "value5";
    ATF_REQUIRE(config == new_config);
}


ATF_TEST_CASE_WITHOUT_HEAD(config__apply_overrides__test_suite__invalid);
ATF_TEST_CASE_BODY(config__apply_overrides__test_suite__invalid)
{
    const user_files::config config = user_files::config::defaults();

    std::vector< user_files::override_pair > overrides;
    overrides.push_back(user_files::override_pair("", ""));

    overrides[0] = user_files::override_pair(".var", "foo");
    ATF_REQUIRE_THROW_RE(std::runtime_error, "Empty test suite.*'.var=foo'",
                         config.apply_overrides(overrides));

    overrides[0] = user_files::override_pair("suite.", "foo");
    ATF_REQUIRE_THROW_RE(std::runtime_error, "Empty property.*'suite.=foo'",
                         config.apply_overrides(overrides));
}


ATF_TEST_CASE_WITHOUT_HEAD(config__test_suite__defined);
ATF_TEST_CASE_BODY(config__test_suite__defined)
{
    user_files::properties_map props1;

    user_files::properties_map props2;
    props2["key1"] = "value1";

    user_files::config config = user_files::config::defaults();
    config.test_suites["suite1"] = props1;
    config.test_suites["suite2"] = props2;

    ATF_REQUIRE(props1 == config.test_suite("suite1"));
    ATF_REQUIRE(props2 == config.test_suite("suite2"));
}


ATF_TEST_CASE_WITHOUT_HEAD(config__test_suite__undefined);
ATF_TEST_CASE_BODY(config__test_suite__undefined)
{
    user_files::properties_map props;
    props["key1"] = "value1";

    user_files::config config = user_files::config::defaults();
    config.test_suites["suite1"] = props;
    config.test_suites["suite2"] = props;

    ATF_REQUIRE(user_files::properties_map() == config.test_suite("suite3"));
}


ATF_TEST_CASE_WITHOUT_HEAD(config__all_properties__minimum);
ATF_TEST_CASE_BODY(config__all_properties__minimum)
{
    const user_files::config config("the-architecture", "the-platform",
                                    none, user_files::test_suites_map());

    user_files::properties_map exp_props;
    exp_props["architecture"] = "the-architecture";
    exp_props["platform"] = "the-platform";

    ATF_REQUIRE(exp_props == config.all_properties());
}


ATF_TEST_CASE_WITHOUT_HEAD(config__all_properties__all);
ATF_TEST_CASE_BODY(config__all_properties__all)
{
    user_files::test_suites_map test_suites;
    test_suites["empty"] = user_files::properties_map();
    {
        user_files::properties_map props;
        props["one"] = "value";
        test_suites["single"] = props;
    }
    {
        user_files::properties_map props;
        props["one"] = "first";
        props["two.three"] = "second";
        test_suites["many"] = props;
    }

    const user_files::config config("the-architecture", "the-platform",
        utils::make_optional(passwd::user("the-user", 100, 150)), test_suites);

    user_files::properties_map exp_props;
    exp_props["architecture"] = "the-architecture";
    exp_props["platform"] = "the-platform";
    exp_props["unprivileged_user"] = "the-user";
    exp_props["single.one"] = "value";
    exp_props["many.one"] = "first";
    exp_props["many.two.three"] = "second";

    ATF_REQUIRE(exp_props == config.all_properties());
}


ATF_TEST_CASE_WITHOUT_HEAD(config__equal_and_different);
ATF_TEST_CASE_BODY(config__equal_and_different)
{
    const user_files::config config = user_files::config::defaults();

    {
        user_files::config other = user_files::config::defaults();
        ATF_REQUIRE(  config == other);
        ATF_REQUIRE(!(config != other));
        other.architecture = "foo";
        ATF_REQUIRE(!(config == other));
        ATF_REQUIRE(  config != other);
    }

    {
        user_files::config other = user_files::config::defaults();
        ATF_REQUIRE(  config == other);
        ATF_REQUIRE(!(config != other));
        other.architecture = "foo";
        ATF_REQUIRE(!(config == other));
        ATF_REQUIRE(  config != other);
    }

    {
        user_files::config other = user_files::config::defaults();
        ATF_REQUIRE(  config == other);
        ATF_REQUIRE(!(config != other));
        other.platform = "foo";
        ATF_REQUIRE(!(config == other));
        ATF_REQUIRE(  config != other);
    }

    {
        user_files::config other = user_files::config::defaults();
        ATF_REQUIRE(  config == other);
        ATF_REQUIRE(!(config != other));
        other.unprivileged_user = passwd::user("user1", 100, 150);
        ATF_REQUIRE(!(config == other));
        ATF_REQUIRE(  config != other);
    }

    {
        user_files::config other = user_files::config::defaults();
        ATF_REQUIRE(  config == other);
        ATF_REQUIRE(!(config != other));
        other.test_suites["hello"]["world"] = "foo";
        ATF_REQUIRE(!(config == other));
        ATF_REQUIRE(  config != other);
    }
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, get_properties__none);
    ATF_ADD_TEST_CASE(tcs, get_properties__some);
    ATF_ADD_TEST_CASE(tcs, get_properties__bad_key);
    ATF_ADD_TEST_CASE(tcs, get_properties__bad_value);

    ATF_ADD_TEST_CASE(tcs, get_string_var__nil);
    ATF_ADD_TEST_CASE(tcs, get_string_var__ok);
    ATF_ADD_TEST_CASE(tcs, get_string_var__invalid);

    ATF_ADD_TEST_CASE(tcs, get_test_suites__none);
    ATF_ADD_TEST_CASE(tcs, get_test_suites__some);
    ATF_ADD_TEST_CASE(tcs, get_test_suites__invalid);
    ATF_ADD_TEST_CASE(tcs, get_test_suites__bad_key);
    ATF_ADD_TEST_CASE(tcs, get_test_suites__bad_value);

    ATF_ADD_TEST_CASE(tcs, get_user_var__nil);
    ATF_ADD_TEST_CASE(tcs, get_user_var__uid_ok);
    ATF_ADD_TEST_CASE(tcs, get_user_var__uid_error);
    ATF_ADD_TEST_CASE(tcs, get_user_var__name_ok);
    ATF_ADD_TEST_CASE(tcs, get_user_var__name_error);
    ATF_ADD_TEST_CASE(tcs, get_user_var__invalid);

    ATF_ADD_TEST_CASE(tcs, get_user_override__uid_ok);
    ATF_ADD_TEST_CASE(tcs, get_user_override__uid_error);
    ATF_ADD_TEST_CASE(tcs, get_user_override__name_ok);
    ATF_ADD_TEST_CASE(tcs, get_user_override__name_error);

    ATF_ADD_TEST_CASE(tcs, config__defaults);
    ATF_ADD_TEST_CASE(tcs, config__load__defaults);
    ATF_ADD_TEST_CASE(tcs, config__load__overrides);
    ATF_ADD_TEST_CASE(tcs, config__load__lua_error);
    ATF_ADD_TEST_CASE(tcs, config__load__bad_syntax__format);
    ATF_ADD_TEST_CASE(tcs, config__load__bad_syntax__version);
    ATF_ADD_TEST_CASE(tcs, config__load__missing_file);
    ATF_ADD_TEST_CASE(tcs, config__apply_overrides__none);
    ATF_ADD_TEST_CASE(tcs, config__apply_overrides__architecture);
    ATF_ADD_TEST_CASE(tcs, config__apply_overrides__platform);
    ATF_ADD_TEST_CASE(tcs, config__apply_overrides__unprivileged_user__ok);
    ATF_ADD_TEST_CASE(tcs, config__apply_overrides__unprivileged_user__invalid);
    ATF_ADD_TEST_CASE(tcs, config__apply_overrides__test_suite__ok);
    ATF_ADD_TEST_CASE(tcs, config__apply_overrides__test_suite__invalid);
    ATF_ADD_TEST_CASE(tcs, config__test_suite__defined);
    ATF_ADD_TEST_CASE(tcs, config__test_suite__undefined);
    ATF_ADD_TEST_CASE(tcs, config__all_properties__minimum);
    ATF_ADD_TEST_CASE(tcs, config__all_properties__all);
    ATF_ADD_TEST_CASE(tcs, config__equal_and_different);
}
