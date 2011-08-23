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

extern "C" {
#include <unistd.h>
}

#include <atf-c++.hpp>

#include "engine/user_files/config.hpp"
#include "engine/user_files/kyuafile.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/passwd.hpp"
#include "utils/test_utils.hpp"

namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace user_files = engine::user_files;


namespace {


/// Path to the directory containing the examples.
static const fs::path examplesdir(KYUA_EXAMPLESDIR);

/// Path to the installed Kyuafile.top file.
static const fs::path installed_kyuafile_top = examplesdir / "Kyuafile.top";

/// Path to the installed kyua.conf file.
static const fs::path installed_kyua_conf = examplesdir / "kyua.conf";


}  // anonymous namespace


ATF_TEST_CASE(kyua_conf);
ATF_TEST_CASE_HEAD(kyua_conf)
{
    set_md_var("require.files", installed_kyua_conf.str());
}
ATF_TEST_CASE_BODY(kyua_conf)
{
    std::vector< passwd::user > users;
    users.push_back(passwd::user("nobody", 1, 2));
    passwd::set_mock_users_for_testing(users);

    const user_files::config config = user_files::config::load(
        installed_kyua_conf);

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


ATF_TEST_CASE(kyuafile_top__no_matches);
ATF_TEST_CASE_HEAD(kyuafile_top__no_matches)
{
    set_md_var("require.files", installed_kyuafile_top.str());
}
ATF_TEST_CASE_BODY(kyuafile_top__no_matches)
{
    fs::mkdir(fs::path("root"), 0755);
    ATF_REQUIRE(::symlink(installed_kyuafile_top.c_str(),
                          "root/Kyuafile") != -1);

    utils::create_file(fs::path("root/file"));
    fs::mkdir(fs::path("root/subdir"), 0755);

    const user_files::kyuafile kyuafile = user_files::kyuafile::load(
        fs::path("root/Kyuafile"));
    ATF_REQUIRE_EQ(fs::path("root"), kyuafile.root());
    ATF_REQUIRE(kyuafile.test_programs().empty());
}


ATF_TEST_CASE(kyuafile_top__some_matches);
ATF_TEST_CASE_HEAD(kyuafile_top__some_matches)
{
    set_md_var("require.files", installed_kyuafile_top.str());
}
ATF_TEST_CASE_BODY(kyuafile_top__some_matches)
{
    fs::mkdir(fs::path("root"), 0755);
    const fs::path source_path(examplesdir / "Kyuafile.top");
    ATF_REQUIRE(::symlink(installed_kyuafile_top.c_str(),
                          "root/Kyuafile") != -1);

    utils::create_file(fs::path("root/file"));

    fs::mkdir(fs::path("root/subdir1"), 0755);
    utils::create_file(fs::path("root/subdir1/Kyuafile"),
                       "syntax('kyuafile', 1)\n"
                       "atf_test_program{name='a', test_suite='b'}\n");
    utils::create_file(fs::path("root/subdir1/a"));

    fs::mkdir(fs::path("root/subdir2"), 0755);
    utils::create_file(fs::path("root/subdir2/Kyuafile"),
                       "syntax('kyuafile', 1)\n"
                       "atf_test_program{name='c', test_suite='d'}\n");
    utils::create_file(fs::path("root/subdir2/c"));
    utils::create_file(fs::path("root/subdir2/Kyuafile.etc"), "invalid");

    const user_files::kyuafile kyuafile = user_files::kyuafile::load(
        fs::path("root/Kyuafile"));
    ATF_REQUIRE_EQ(fs::path("root"), kyuafile.root());
    ATF_REQUIRE_EQ(2, kyuafile.test_programs().size());
    if (kyuafile.test_programs()[0]->relative_path() == fs::path("subdir1/a")) {
        ATF_REQUIRE_EQ("b", kyuafile.test_programs()[0]->test_suite_name());
        ATF_REQUIRE_EQ(fs::path("subdir2/c"),
                       kyuafile.test_programs()[1]->relative_path());
        ATF_REQUIRE_EQ("d", kyuafile.test_programs()[1]->test_suite_name());
    } else {
        ATF_REQUIRE_EQ(fs::path("subdir2/c"),
                       kyuafile.test_programs()[0]->relative_path());
        ATF_REQUIRE_EQ("d", kyuafile.test_programs()[0]->test_suite_name());
        ATF_REQUIRE_EQ(fs::path("subdir1/a"),
                       kyuafile.test_programs()[1]->relative_path());
        ATF_REQUIRE_EQ("b", kyuafile.test_programs()[1]->test_suite_name());
    }
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, kyua_conf);

    ATF_ADD_TEST_CASE(tcs, kyuafile_top__no_matches);
    ATF_ADD_TEST_CASE(tcs, kyuafile_top__some_matches);
}
