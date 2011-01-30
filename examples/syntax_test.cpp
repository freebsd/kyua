// Copyright 2011 Google Inc.
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

#include "engine/user_files/config.hpp"
#include "utils/fs/path.hpp"
#include "utils/passwd.hpp"

namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace user_files = engine::user_files;


namespace {


/// Path to the directory containing the examples.
static const fs::path examplesdir(KYUA_EXAMPLESDIR);


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(kyua_conf);
ATF_TEST_CASE_BODY(kyua_conf)
{
    std::vector< passwd::user > users;
    users.push_back(passwd::user("nobody", 1, 2));
    passwd::set_mock_users_for_testing(users);

    const user_files::config config = user_files::config::load(
        examplesdir / "kyua.conf");

    ATF_REQUIRE_EQ("x86_64", config.architecture);
    ATF_REQUIRE_EQ("amd64", config.platform);

    ATF_REQUIRE(config.unprivileged_user);
    ATF_REQUIRE_EQ("nobody", config.unprivileged_user.get().name);

    ATF_REQUIRE_EQ(2, config.test_suites.size());
    {
        const user_files::properties_map& properties =
            config.test_suite("FreeBSD");
        ATF_REQUIRE_EQ(2, properties.size());

        user_files::properties_map::const_iterator iter;

        iter = properties.find("iterations");
        ATF_REQUIRE(iter != properties.end());
        ATF_REQUIRE_EQ("1000", (*iter).second);

        iter = properties.find("run_old_tests");
        ATF_REQUIRE(iter != properties.end());
        ATF_REQUIRE_EQ("false", (*iter).second);
    }
    {
        const user_files::properties_map& properties =
            config.test_suite("NetBSD");
        ATF_REQUIRE_EQ(3, properties.size());

        user_files::properties_map::const_iterator iter;

        iter = properties.find("file_systems");
        ATF_REQUIRE(iter != properties.end());
        ATF_REQUIRE_EQ("ffs lfs ext2fs", (*iter).second);

        iter = properties.find("iterations");
        ATF_REQUIRE(iter != properties.end());
        ATF_REQUIRE_EQ("100", (*iter).second);

        iter = properties.find("run_broken_tests");
        ATF_REQUIRE(iter != properties.end());
        ATF_REQUIRE_EQ("true", (*iter).second);
    }
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, kyua_conf);
}
