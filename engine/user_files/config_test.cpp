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

#if defined(HAVE_CONFIG_H)
#   include "config.h"
#endif

#include <fstream>
#include <vector>

#include <atf-c++.hpp>

#include "engine/exceptions.hpp"
#include "engine/user_files/config.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/lua/operations.hpp"
#include "utils/lua/test_utils.hpp"
#include "utils/lua/wrap.ipp"
#include "utils/passwd.hpp"

namespace fs = utils::fs;
namespace lua = utils::lua;
namespace passwd = utils::passwd;
namespace user_files = engine::user_files;

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
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(get_string_var__nil);
ATF_TEST_CASE_BODY(get_string_var__nil)
{
    lua::state state;
    stack_balance_checker checker(state);
    ATF_REQUIRE_EQ("default-value", user_files::detail::get_string_var(
        state, "undefined", "default-value"));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_string_var__ok);
ATF_TEST_CASE_BODY(get_string_var__ok)
{
    lua::state state;
    lua::do_string(state, "myvar = 'foo bar baz'");

    stack_balance_checker checker(state);
    ATF_REQUIRE_EQ("foo bar baz", user_files::detail::get_string_var(
        state, "myvar", "default-value"));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_string_var__invalid);
ATF_TEST_CASE_BODY(get_string_var__invalid)
{
    lua::state state;
    lua::do_string(state, "myvar = {}");

    stack_balance_checker checker(state);
    ATF_REQUIRE_THROW_RE(engine::error, "Invalid type.*'myvar'",
                         user_files::detail::get_string_var(state, "myvar",
                                                            "default-value"));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_user_var__nil);
ATF_TEST_CASE_BODY(get_user_var__nil)
{
    set_mock_users();

    lua::state state;
    stack_balance_checker checker(state);
    ATF_REQUIRE(!user_files::detail::get_user_var(state, "undefined"));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_user_var__uid_ok);
ATF_TEST_CASE_BODY(get_user_var__uid_ok)
{
    set_mock_users();

    lua::state state;
    lua::do_string(state, "myvar = 100");

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

    lua::state state;
    lua::do_string(state, "myvar = '150'");

    stack_balance_checker checker(state);
    ATF_REQUIRE_THROW_RE(engine::error, "Cannot find user.*UID 150.*'myvar'",
                         user_files::detail::get_user_var(state, "myvar"));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_user_var__name_ok);
ATF_TEST_CASE_BODY(get_user_var__name_ok)
{
    set_mock_users();

    lua::state state;
    lua::do_string(state, "myvar = 'user2'");

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

    lua::state state;
    lua::do_string(state, "myvar = 'root'");

    stack_balance_checker checker(state);
    ATF_REQUIRE_THROW_RE(engine::error, "Cannot find user.*name 'root'.*'myvar'",
                         user_files::detail::get_user_var(state, "myvar"));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_user_var__invalid);
ATF_TEST_CASE_BODY(get_user_var__invalid)
{
    set_mock_users();

    lua::state state;
    lua::do_string(state, "myvar = {}");

    stack_balance_checker checker(state);
    ATF_REQUIRE_THROW_RE(engine::error, "Invalid type.*'myvar'",
                         user_files::detail::get_user_var(state, "myvar"));
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
        file.close();
    }

    const user_files::config config = user_files::config::load(
        fs::path("config"));
    ATF_REQUIRE_EQ("test-architecture", config.architecture);
    ATF_REQUIRE_EQ("test-platform", config.platform);
    ATF_REQUIRE(config.unprivileged_user);
    ATF_REQUIRE_EQ("user2", config.unprivileged_user.get().name);
    ATF_REQUIRE_EQ(200, config.unprivileged_user.get().uid);
}


ATF_TEST_CASE_WITHOUT_HEAD(config__load__lua_error);
ATF_TEST_CASE_BODY(config__load__lua_error)
{
    std::ofstream file("config");
    file << "this syntax is invalid\n";
    file.close();

    ATF_REQUIRE_THROW(engine::error, user_files::config::load(
        fs::path("config")));
}


ATF_TEST_CASE_WITHOUT_HEAD(config__load__bad_syntax__format);
ATF_TEST_CASE_BODY(config__load__bad_syntax__format)
{
    std::ofstream file("config");
    file << "syntax('config', 1)\n";
    file << "init.get_syntax().format = 'foo'\n";
    file.close();

    ATF_REQUIRE_THROW_RE(engine::error, "Unexpected file format 'foo'",
                         user_files::config::load(fs::path("config")));
}


ATF_TEST_CASE_WITHOUT_HEAD(config__load__bad_syntax__version);
ATF_TEST_CASE_BODY(config__load__bad_syntax__version)
{
    std::ofstream file("config");
    file << "syntax('config', 1)\n";
    file << "init.get_syntax().version = 123\n";
    file.close();

    ATF_REQUIRE_THROW_RE(engine::error, "Unexpected file version '123'",
                         user_files::config::load(fs::path("config")));
}


ATF_TEST_CASE_WITHOUT_HEAD(config__load__missing_file);
ATF_TEST_CASE_BODY(config__load__missing_file)
{
    ATF_REQUIRE_THROW_RE(engine::error, "Load failed",
                         user_files::config::load(fs::path("missing")));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, get_string_var__nil);
    ATF_ADD_TEST_CASE(tcs, get_string_var__ok);
    ATF_ADD_TEST_CASE(tcs, get_string_var__invalid);

    ATF_ADD_TEST_CASE(tcs, get_user_var__nil);
    ATF_ADD_TEST_CASE(tcs, get_user_var__uid_ok);
    ATF_ADD_TEST_CASE(tcs, get_user_var__uid_error);
    ATF_ADD_TEST_CASE(tcs, get_user_var__name_ok);
    ATF_ADD_TEST_CASE(tcs, get_user_var__name_error);
    ATF_ADD_TEST_CASE(tcs, get_user_var__invalid);

    ATF_ADD_TEST_CASE(tcs, config__defaults);
    ATF_ADD_TEST_CASE(tcs, config__load__defaults);
    ATF_ADD_TEST_CASE(tcs, config__load__overrides);
    ATF_ADD_TEST_CASE(tcs, config__load__lua_error);
    ATF_ADD_TEST_CASE(tcs, config__load__bad_syntax__format);
    ATF_ADD_TEST_CASE(tcs, config__load__bad_syntax__version);
    ATF_ADD_TEST_CASE(tcs, config__load__missing_file);
}
