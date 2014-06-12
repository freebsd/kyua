// Copyright 2014 Google Inc.
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

#include "store/layout.hpp"

extern "C" {
#include <unistd.h>
}

#include <iostream>

#include <atf-c++.hpp>

#include "store/exceptions.hpp"
#include "store/layout.hpp"
#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace layout = store::layout;


ATF_TEST_CASE_WITHOUT_HEAD(find_latest__time);
ATF_TEST_CASE_BODY(find_latest__time)
{
    const fs::path store_dir = layout::query_store_dir();
    fs::mkdir_p(store_dir, 0755);

    atf::utils::create_file(
        (store_dir / "kyua.the_suite.20140613-194515.db").str(), "");
    ATF_REQUIRE_EQ(store_dir / "kyua.the_suite.20140613-194515.db",
                   layout::find_latest("the_suite"));

    atf::utils::create_file(
        (store_dir / "kyua.the_suite.20140614-194515.db").str(), "");
    ATF_REQUIRE_EQ(store_dir / "kyua.the_suite.20140614-194515.db",
                   layout::find_latest("the_suite"));

    atf::utils::create_file(
        (store_dir / "kyua.the_suite.20130614-194515.db").str(), "");
    ATF_REQUIRE_EQ(store_dir / "kyua.the_suite.20140614-194515.db",
                   layout::find_latest("the_suite"));
}


ATF_TEST_CASE_WITHOUT_HEAD(find_latest__not_found);
ATF_TEST_CASE_BODY(find_latest__not_found)
{
    ATF_REQUIRE_THROW_RE(store::error,
                         "No previous action found for test suite foo_bar",
                         layout::find_latest("foo_bar"));

    const fs::path store_dir = layout::query_store_dir();
    fs::mkdir_p(store_dir, 0755);
    ATF_REQUIRE_THROW_RE(store::error,
                         "No previous action found for test suite foo_bar",
                         layout::find_latest("foo_bar"));

    const char* candidates[] = {
        "kyua.foo.20140613-194515.db",  // Bad test suite.
        "kyua.foo_bar.20140613-194515",  // Missing extension.
        "foo_bar.20140613-194515.db",  // Missing prefix.
        "kyua.foo_bar.2010613-194515.db",  // Bad date.
        "kyua.foo_bar.20140613-19515.db",  // Bad time.
        NULL,
    };
    for (const char** candidate = candidates; *candidate != NULL; ++candidate) {
        std::cout << "Current candidate: " << *candidate << '\n';
        atf::utils::create_file((store_dir / *candidate).str(), "");
        ATF_REQUIRE_THROW_RE(store::error,
                             "No previous action found for test suite foo_bar",
                             layout::find_latest("foo_bar"));
    }

    atf::utils::create_file(
        (store_dir / "kyua.foo_bar.20140613-194515.db").str(), "");
    layout::find_latest("foo_bar");  // Expected not to throw.
}


ATF_TEST_CASE_WITHOUT_HEAD(new_db__ok);
ATF_TEST_CASE_BODY(new_db__ok)
{
    datetime::set_mock_now(2014, 6, 13, 19, 45, 15, 5000);
    ATF_REQUIRE_EQ(
        layout::query_store_dir() / "kyua.the_suite.20140613-194515.db",
        layout::new_db("the_suite"));
}


ATF_TEST_CASE_WITHOUT_HEAD(new_db__already_exists);
ATF_TEST_CASE_BODY(new_db__already_exists)
{
    datetime::set_mock_now(2014, 6, 13, 19, 45, 15, 5000);
    const fs::path db = layout::new_db("the_suite");
    fs::mkdir_p(db.branch_path(), 0755);
    atf::utils::create_file(db.str(), "");
    ATF_REQUIRE_THROW_RE(store::error, "already exists",
                         layout::new_db("the_suite"));
}


ATF_TEST_CASE_WITHOUT_HEAD(query_store_dir__home_absolute);
ATF_TEST_CASE_BODY(query_store_dir__home_absolute)
{
    const fs::path home = fs::current_path() / "homedir";
    utils::setenv("HOME", home.str());
    const fs::path store_dir = layout::query_store_dir();
    ATF_REQUIRE(store_dir.is_absolute());
    ATF_REQUIRE_EQ(home / ".kyua/actions", store_dir);
}


ATF_TEST_CASE_WITHOUT_HEAD(query_store_dir__home_relative);
ATF_TEST_CASE_BODY(query_store_dir__home_relative)
{
    const fs::path home("homedir");
    utils::setenv("HOME", home.str());
    const fs::path store_dir = layout::query_store_dir();
    ATF_REQUIRE(store_dir.is_absolute());
    ATF_REQUIRE_MATCH((home / ".kyua/actions").str(), store_dir.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(query_store_dir__no_home);
ATF_TEST_CASE_BODY(query_store_dir__no_home)
{
    utils::unsetenv("HOME");
    ATF_REQUIRE_EQ(fs::current_path(), layout::query_store_dir());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_suite_for_path__absolute);
ATF_TEST_CASE_BODY(test_suite_for_path__absolute)
{
    ATF_REQUIRE_EQ("dir1_dir2_dir3",
                   layout::test_suite_for_path(fs::path("/dir1/dir2/dir3")));
    ATF_REQUIRE_EQ("dir1",
                   layout::test_suite_for_path(fs::path("/dir1")));
    ATF_REQUIRE_EQ("dir1_dir2",
                   layout::test_suite_for_path(fs::path("/dir1///dir2")));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_suite_for_path__relative);
ATF_TEST_CASE_BODY(test_suite_for_path__relative)
{
    const std::string test_suite = layout::test_suite_for_path(
        fs::path("foo/bar"));
    ATF_REQUIRE_MATCH("_foo_bar$", test_suite);
    ATF_REQUIRE_MATCH("^[^_]", test_suite);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, find_latest__time);
    ATF_ADD_TEST_CASE(tcs, find_latest__not_found);

    ATF_ADD_TEST_CASE(tcs, new_db__ok);
    ATF_ADD_TEST_CASE(tcs, new_db__already_exists);

    ATF_ADD_TEST_CASE(tcs, query_store_dir__home_absolute);
    ATF_ADD_TEST_CASE(tcs, query_store_dir__home_relative);
    ATF_ADD_TEST_CASE(tcs, query_store_dir__no_home);

    ATF_ADD_TEST_CASE(tcs, test_suite_for_path__absolute);
    ATF_ADD_TEST_CASE(tcs, test_suite_for_path__relative);
}
