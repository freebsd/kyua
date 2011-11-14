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

#include <fstream>

#include <atf-c++.hpp>

#include "store/backend.hpp"
#include "store/exceptions.hpp"
#include "store/metadata.hpp"
#include "utils/datetime.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/statement.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace sqlite = utils::sqlite;


ATF_TEST_CASE(detail_initialize__ok);
ATF_TEST_CASE_HEAD(detail_initialize__ok)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(detail_initialize__ok)
{
    sqlite::database db = sqlite::database::in_memory();
    const datetime::timestamp before = datetime::timestamp::now();
    const store::metadata md = store::detail::initialize(db);
    const datetime::timestamp after = datetime::timestamp::now();

    ATF_REQUIRE(md.timestamp() >= before.timegm());
    ATF_REQUIRE(md.timestamp() <= after.timegm());
    ATF_REQUIRE_EQ(store::detail::current_schema_version, md.schema_version());

    // Query some known tables to ensure they were created.
    db.exec("SELECT * FROM metadata");

    // And now query some know values.
    sqlite::statement stmt = db.create_statement(
        "SELECT COUNT(*) FROM metadata");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(1, stmt.column_int(0));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(detail_initialize__missing_schema);
ATF_TEST_CASE_BODY(detail_initialize__missing_schema)
{
    sqlite::database db = sqlite::database::in_memory();
    ATF_REQUIRE_THROW_RE(store::error, "Cannot open.*'abc.sql'",
                         store::detail::initialize(db, fs::path("abc.sql")));
}


ATF_TEST_CASE_WITHOUT_HEAD(detail_initialize__sqlite_error);
ATF_TEST_CASE_BODY(detail_initialize__sqlite_error)
{
    std::ofstream output("schema.sql");
    output << "foo_bar_baz;\n";
    output.close();

    sqlite::database db = sqlite::database::in_memory();
    ATF_REQUIRE_THROW_RE(store::error, "Failed to initialize.*:.*foo_bar_baz",
                         store::detail::initialize(db, fs::path("schema.sql")));
}


ATF_TEST_CASE(backend__open_ro__ok);
ATF_TEST_CASE_HEAD(backend__open_ro__ok)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(backend__open_ro__ok)
{
    {
        sqlite::database db = sqlite::database::open(
            fs::path("test.db"), sqlite::open_readwrite | sqlite::open_create);
        store::detail::initialize(db);
    }
    store::backend backend = store::backend::open_ro(fs::path("test.db"));
    backend.database().exec("SELECT * FROM metadata");
}


ATF_TEST_CASE_WITHOUT_HEAD(backend__open_ro__missing_file);
ATF_TEST_CASE_BODY(backend__open_ro__missing_file)
{
    ATF_REQUIRE_THROW_RE(store::error, "Cannot open 'missing.db': ",
                         store::backend::open_ro(fs::path("missing.db")));
    ATF_REQUIRE(!fs::exists(fs::path("missing.db")));
}


ATF_TEST_CASE(backend__open_ro__integrity_error);
ATF_TEST_CASE_HEAD(backend__open_ro__integrity_error)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(backend__open_ro__integrity_error)
{
    {
        sqlite::database db = sqlite::database::open(
            fs::path("test.db"), sqlite::open_readwrite | sqlite::open_create);
        store::detail::initialize(db);
        db.exec("DELETE FROM metadata");
    }
    ATF_REQUIRE_THROW_RE(store::integrity_error, "metadata.*empty",
                         store::backend::open_ro(fs::path("test.db")));
}


ATF_TEST_CASE(backend__open_rw__ok);
ATF_TEST_CASE_HEAD(backend__open_rw__ok)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(backend__open_rw__ok)
{
    {
        sqlite::database db = sqlite::database::open(
            fs::path("test.db"), sqlite::open_readwrite | sqlite::open_create);
        store::detail::initialize(db);
    }
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    backend.database().exec("SELECT * FROM metadata");
}


ATF_TEST_CASE_WITHOUT_HEAD(backend__open_rw__create_missing);
ATF_TEST_CASE_BODY(backend__open_rw__create_missing)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    backend.database().exec("SELECT * FROM metadata");
}


ATF_TEST_CASE(backend__open_rw__integrity_error);
ATF_TEST_CASE_HEAD(backend__open_rw__integrity_error)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(backend__open_rw__integrity_error)
{
    {
        sqlite::database db = sqlite::database::open(
            fs::path("test.db"), sqlite::open_readwrite | sqlite::open_create);
        store::detail::initialize(db);
        db.exec("DELETE FROM metadata");
    }
    ATF_REQUIRE_THROW_RE(store::integrity_error, "metadata.*empty",
                         store::backend::open_rw(fs::path("test.db")));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, detail_initialize__ok);
    ATF_ADD_TEST_CASE(tcs, detail_initialize__missing_schema);
    ATF_ADD_TEST_CASE(tcs, detail_initialize__sqlite_error);

    ATF_ADD_TEST_CASE(tcs, backend__open_ro__ok);
    ATF_ADD_TEST_CASE(tcs, backend__open_ro__missing_file);
    ATF_ADD_TEST_CASE(tcs, backend__open_ro__integrity_error);
    ATF_ADD_TEST_CASE(tcs, backend__open_rw__ok);
    ATF_ADD_TEST_CASE(tcs, backend__open_rw__create_missing);
    ATF_ADD_TEST_CASE(tcs, backend__open_rw__integrity_error);
}
