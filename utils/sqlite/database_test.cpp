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

#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/test_utils.hpp"

namespace fs = utils::fs;
namespace sqlite = utils::sqlite;


ATF_TEST_CASE_WITHOUT_HEAD(open__readonly__ok);
ATF_TEST_CASE_BODY(open__readonly__ok)
{
    {
        ::sqlite3* db;
        ATF_REQUIRE_EQ(SQLITE_OK, ::sqlite3_open_v2("test.db", &db,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL));
        create_test_table(db);
        ::sqlite3_close(db);
    }
    {
        sqlite::database db = sqlite::database::open(fs::path("test.db"),
            sqlite::open_readonly);
        verify_test_table(raw(db));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(open__readonly__fail);
ATF_TEST_CASE_BODY(open__readonly__fail)
{
    REQUIRE_API_ERROR("sqlite3_open_v2",
        sqlite::database::open(fs::path("missing.db"), sqlite::open_readonly));
    ATF_REQUIRE(!fs::exists(fs::path("missing.db")));
}


ATF_TEST_CASE_WITHOUT_HEAD(open__create__ok);
ATF_TEST_CASE_BODY(open__create__ok)
{
    {
        sqlite::database db = sqlite::database::open(fs::path("test.db"),
            sqlite::open_readwrite | sqlite::open_create);
        ATF_REQUIRE(fs::exists(fs::path("test.db")));
        create_test_table(raw(db));
    }
    {
        ::sqlite3* db;
        ATF_REQUIRE_EQ(SQLITE_OK, ::sqlite3_open_v2("test.db", &db,
            SQLITE_OPEN_READONLY, NULL));
        verify_test_table(db);
        ::sqlite3_close(db);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(open__create__fail);
ATF_TEST_CASE_BODY(open__create__fail)
{
    fs::mkdir(fs::path("protected"), 0555);
    REQUIRE_API_ERROR("sqlite3_open_v2",
        sqlite::database::open(fs::path("protected/test.db"),
                               sqlite::open_readwrite | sqlite::open_create));
}


ATF_TEST_CASE_WITHOUT_HEAD(close);
ATF_TEST_CASE_BODY(close)
{
    sqlite::database db = sqlite::database::open(fs::path(":memory:"),
        sqlite::open_readonly);
    ATF_REQUIRE(!fs::exists(fs::path(":memory:")));
    db.close();
    // The destructor for the database will run now.  If it does a second close,
    // we may crash, so let's see if we don't.
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, open__readonly__ok);
    ATF_ADD_TEST_CASE(tcs, open__readonly__fail);
    ATF_ADD_TEST_CASE(tcs, open__create__ok);
    ATF_ADD_TEST_CASE(tcs, open__create__fail);

    ATF_ADD_TEST_CASE(tcs, close);
}
