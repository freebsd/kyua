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
#include <stdint.h>
}

#include <iostream>

#include <atf-c++.hpp>

#include "utils/sqlite/database.hpp"
#include "utils/sqlite/statement.hpp"
#include "utils/sqlite/test_utils.hpp"

namespace sqlite = utils::sqlite;


ATF_TEST_CASE_WITHOUT_HEAD(step__ok);
ATF_TEST_CASE_BODY(step__ok)
{
    sqlite::database db = sqlite::database::in_memory();
    sqlite::statement stmt = db.create_statement(
        "CREATE TABLE foo (a INTEGER PRIMARY KEY)");
    ATF_REQUIRE_THROW(sqlite::error, db.exec("SELECT * FROM foo"));
    ATF_REQUIRE(!stmt.step());
    db.exec("SELECT * FROM foo");
}


ATF_TEST_CASE_WITHOUT_HEAD(step__many);
ATF_TEST_CASE_BODY(step__many)
{
    sqlite::database db = sqlite::database::in_memory();
    create_test_table(raw(db));
    sqlite::statement stmt = db.create_statement(
        "SELECT prime FROM test ORDER BY prime");
    for (int i = 0; i < 5; i++)
        ATF_REQUIRE(stmt.step());
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(step__fail);
ATF_TEST_CASE_BODY(step__fail)
{
    sqlite::database db = sqlite::database::in_memory();
    sqlite::statement stmt = db.create_statement(
        "CREATE TABLE foo (a INTEGER PRIMARY KEY)");
    ATF_REQUIRE(!stmt.step());
    REQUIRE_API_ERROR("sqlite3_step", stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_count);
ATF_TEST_CASE_BODY(column_count)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a INTEGER PRIMARY KEY, b INTEGER, c TEXT);"
            "INSERT INTO foo VALUES (5, 3, 'foo');");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(3, stmt.column_count());
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_name__ok);
ATF_TEST_CASE_BODY(column_name__ok)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (first INTEGER PRIMARY KEY, second TEXT);"
            "INSERT INTO foo VALUES (5, 'foo');");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ("first", stmt.column_name(0));
    ATF_REQUIRE_EQ("second", stmt.column_name(1));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_name__fail);
ATF_TEST_CASE_BODY(column_name__fail)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (first INTEGER PRIMARY KEY);"
            "INSERT INTO foo VALUES (5);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ("first", stmt.column_name(0));
    REQUIRE_API_ERROR("sqlite3_column_name", stmt.column_name(1));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_type__ok);
ATF_TEST_CASE_BODY(column_type__ok)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a_blob BLOB,"
            "                  a_float FLOAT,"
            "                  an_integer INTEGER,"
            "                  a_null BLOB,"
            "                  a_text TEXT);"
            "INSERT INTO foo VALUES (x'0102', 0.3, 5, NULL, 'foo bar');"
            "INSERT INTO foo VALUES (NULL, NULL, NULL, NULL, NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(sqlite::type_blob, stmt.column_type(0));
    ATF_REQUIRE_EQ(sqlite::type_float, stmt.column_type(1));
    ATF_REQUIRE_EQ(sqlite::type_integer, stmt.column_type(2));
    ATF_REQUIRE_EQ(sqlite::type_null, stmt.column_type(3));
    ATF_REQUIRE_EQ(sqlite::type_text, stmt.column_type(4));
    ATF_REQUIRE(stmt.step());
    for (int i = 0; i < stmt.column_count(); i++)
        ATF_REQUIRE_EQ(sqlite::type_null, stmt.column_type(i));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_type__out_of_range);
ATF_TEST_CASE_BODY(column_type__out_of_range)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a INTEGER PRIMARY KEY);"
            "INSERT INTO foo VALUES (1);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(sqlite::type_integer, stmt.column_type(0));
    ATF_REQUIRE_EQ(sqlite::type_null, stmt.column_type(1));
    ATF_REQUIRE_EQ(sqlite::type_null, stmt.column_type(512));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_blob);
ATF_TEST_CASE_BODY(column_blob)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a INTEGER, b BLOB, c INTEGER);"
            "INSERT INTO foo VALUES (NULL, x'cafe', NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    const void* blob = stmt.column_blob(1);
    ATF_REQUIRE_EQ(0xca, static_cast< const uint8_t* >(blob)[0]);
    ATF_REQUIRE_EQ(0xfe, static_cast< const uint8_t* >(blob)[1]);
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_double);
ATF_TEST_CASE_BODY(column_double)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a INTEGER, b DOUBLE, c INTEGER);"
            "INSERT INTO foo VALUES (NULL, 0.5, NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(0.5, stmt.column_double(1));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_int__ok);
ATF_TEST_CASE_BODY(column_int__ok)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a TEXT, b INTEGER, c TEXT);"
            "INSERT INTO foo VALUES (NULL, 987, NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(987, stmt.column_int(1));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_int__overflow);
ATF_TEST_CASE_BODY(column_int__overflow)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a TEXT, b INTEGER, c TEXT);"
            "INSERT INTO foo VALUES (NULL, 4294967419, NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(123, stmt.column_int(1));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_int64);
ATF_TEST_CASE_BODY(column_int64)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a TEXT, b INTEGER, c TEXT);"
            "INSERT INTO foo VALUES (NULL, 4294967419, NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(4294967419L, stmt.column_int64(1));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_text);
ATF_TEST_CASE_BODY(column_text)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a INTEGER, b TEXT, c INTEGER);"
            "INSERT INTO foo VALUES (NULL, 'foo bar', NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ("foo bar", std::string(stmt.column_text(1)));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_bytes__blob);
ATF_TEST_CASE_BODY(column_bytes__blob)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a BLOB);"
            "INSERT INTO foo VALUES (x'12345678');");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(4, stmt.column_bytes(0));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_bytes__text);
ATF_TEST_CASE_BODY(column_bytes__text)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a TEXT);"
            "INSERT INTO foo VALUES ('foo bar');");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(7, stmt.column_bytes(0));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(reset);
ATF_TEST_CASE_BODY(reset)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a TEXT);"
            "INSERT INTO foo VALUES ('foo bar');");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE(!stmt.step());
    stmt.reset();
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE(!stmt.step());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, step__ok);
    ATF_ADD_TEST_CASE(tcs, step__many);
    ATF_ADD_TEST_CASE(tcs, step__fail);

    ATF_ADD_TEST_CASE(tcs, column_count);

    ATF_ADD_TEST_CASE(tcs, column_name__ok);
    ATF_ADD_TEST_CASE(tcs, column_name__fail);

    ATF_ADD_TEST_CASE(tcs, column_type__ok);
    ATF_ADD_TEST_CASE(tcs, column_type__out_of_range);

    ATF_ADD_TEST_CASE(tcs, column_blob);
    ATF_ADD_TEST_CASE(tcs, column_double);
    ATF_ADD_TEST_CASE(tcs, column_int__ok);
    ATF_ADD_TEST_CASE(tcs, column_int__overflow);
    ATF_ADD_TEST_CASE(tcs, column_int64);
    ATF_ADD_TEST_CASE(tcs, column_text);

    ATF_ADD_TEST_CASE(tcs, column_bytes__blob);
    ATF_ADD_TEST_CASE(tcs, column_bytes__text);

    ATF_ADD_TEST_CASE(tcs, reset);
}
