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

#include "store/write_transaction.hpp"

#include <cstring>
#include <map>
#include <string>

#include <atf-c++.hpp>

#include "engine/action.hpp"
#include "engine/context.hpp"
#include "engine/test_result.hpp"
#include "store/backend.hpp"
#include "store/exceptions.hpp"
#include "utils/datetime.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/operations.hpp"
#include "utils/optional.ipp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.ipp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace logging = utils::logging;
namespace sqlite = utils::sqlite;

using utils::optional;


namespace {


/// Performs a test for a working put_result
///
/// \param result The result object to put.
/// \param result_type The textual name of the result to expect in the
///     database.
/// \param exp_reason The reason to expect in the database.  This is separate
///     from the result parameter so that we can handle passed() here as well.
///     Just provide NULL in this case.
static void
do_put_result_ok_test(const engine::test_result& result,
                      const char* result_type, const char* exp_reason)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    backend.database().exec("PRAGMA foreign_keys = OFF");
    store::write_transaction tx = backend.start_write();
    const datetime::timestamp start_time = datetime::timestamp::from_values(
        2012, 01, 30, 22, 10, 00, 0);
    const datetime::timestamp end_time = datetime::timestamp::from_values(
        2012, 01, 30, 22, 15, 30, 123456);
    tx.put_result(result, 312, start_time, end_time);
    tx.commit();

    sqlite::statement stmt = backend.database().create_statement(
        "SELECT test_case_id, result_type, result_reason "
        "FROM test_results");

    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(312, stmt.column_int64(0));
    ATF_REQUIRE_EQ(result_type, stmt.column_text(1));
    if (exp_reason != NULL)
        ATF_REQUIRE_EQ(exp_reason, stmt.column_text(2));
    else
        ATF_REQUIRE(stmt.column_type(2) == sqlite::type_null);
    ATF_REQUIRE(!stmt.step());
}


}  // anonymous namespace


ATF_TEST_CASE(commit__ok);
ATF_TEST_CASE_HEAD(commit__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(commit__ok)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::write_transaction tx = backend.start_write();
    backend.database().exec("CREATE TABLE a (b INTEGER PRIMARY KEY)");
    backend.database().exec("SELECT * FROM a");
    tx.commit();
    backend.database().exec("SELECT * FROM a");
}


ATF_TEST_CASE(commit__fail);
ATF_TEST_CASE_HEAD(commit__fail)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(commit__fail)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    const engine::context context(fs::path("/foo/bar"),
                                  std::map< std::string, std::string >());
    {
        store::write_transaction tx = backend.start_write();
        tx.put_context(context);
        backend.database().exec(
            "CREATE TABLE foo ("
            "a REFERENCES contexts(context_id) DEFERRABLE INITIALLY DEFERRED)");
        backend.database().exec("INSERT INTO foo VALUES (912378472)");
        ATF_REQUIRE_THROW(store::error, tx.commit());
    }
    // If the code attempts to maintain any state regarding the already-put
    // objects and the commit does not clean up correctly, this would fail in
    // some manner.
    store::write_transaction tx = backend.start_write();
    tx.put_context(context);
    tx.commit();
}


ATF_TEST_CASE(rollback__ok);
ATF_TEST_CASE_HEAD(rollback__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(rollback__ok)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::write_transaction tx = backend.start_write();
    backend.database().exec("CREATE TABLE a_table (b INTEGER PRIMARY KEY)");
    backend.database().exec("SELECT * FROM a_table");
    tx.rollback();
    ATF_REQUIRE_THROW_RE(sqlite::error, "a_table",
                         backend.database().exec("SELECT * FROM a_table"));
}


ATF_TEST_CASE(put_action__fail);
ATF_TEST_CASE_HEAD(put_action__fail)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_action__fail)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::write_transaction tx = backend.start_write();
    const engine::context context(fs::path("/foo/bar"),
                                  std::map< std::string, std::string >());
    const int64_t context_id = tx.put_context(context);
    const engine::action action(context);
    backend.database().exec("DROP TABLE actions");
    ATF_REQUIRE_THROW(store::error, tx.put_action(action, context_id));
    tx.commit();
}


ATF_TEST_CASE(put_context__fail);
ATF_TEST_CASE_HEAD(put_context__fail)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_context__fail)
{
    (void)store::backend::open_rw(fs::path("test.db"));
    store::backend backend = store::backend::open_ro(fs::path("test.db"));
    store::write_transaction tx = backend.start_write();
    const engine::context context(fs::path("/foo/bar"),
                                  std::map< std::string, std::string >());
    ATF_REQUIRE_THROW(store::error, tx.put_context(context));
    tx.commit();
}


ATF_TEST_CASE(put_test_program__ok);
ATF_TEST_CASE_HEAD(put_test_program__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_test_program__ok)
{
    const engine::metadata md = engine::metadata_builder()
        .add_custom("var1", "value1")
        .add_custom("var2", "value2")
        .build();
    const engine::test_program test_program(
        "mock", fs::path("the/binary"), fs::path("/some//root"),
        "the-suite", md);

    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    backend.database().exec("PRAGMA foreign_keys = OFF");
    store::write_transaction tx = backend.start_write();
    const int64_t test_program_id = tx.put_test_program(test_program, 15);
    tx.commit();

    {
        sqlite::statement stmt = backend.database().create_statement(
            "SELECT * FROM test_programs");

        ATF_REQUIRE(stmt.step());
        ATF_REQUIRE_EQ(test_program_id,
                       stmt.safe_column_int64("test_program_id"));
        ATF_REQUIRE_EQ(15, stmt.safe_column_int64("action_id"));
        ATF_REQUIRE_EQ("/some/root/the/binary",
                       stmt.safe_column_text("absolute_path"));
        ATF_REQUIRE_EQ("/some/root", stmt.safe_column_text("root"));
        ATF_REQUIRE_EQ("the/binary", stmt.safe_column_text("relative_path"));
        ATF_REQUIRE_EQ("the-suite", stmt.safe_column_text("test_suite_name"));
        ATF_REQUIRE(!stmt.step());
    }
}


ATF_TEST_CASE(put_test_program__fail);
ATF_TEST_CASE_HEAD(put_test_program__fail)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_test_program__fail)
{
    const engine::test_program test_program(
        "mock", fs::path("the/binary"), fs::path("/some/root"), "the-suite",
        engine::metadata_builder().build());

    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::write_transaction tx = backend.start_write();
    ATF_REQUIRE_THROW(store::error, tx.put_test_program(test_program, -1));
    tx.commit();
}


ATF_TEST_CASE(put_test_case__fail);
ATF_TEST_CASE_HEAD(put_test_case__fail)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_test_case__fail)
{
    // TODO(jmmv): Use a mock test program and test case.
    const engine::test_program test_program(
        "plain", fs::path("the/binary"), fs::path("/some/root"), "the-suite",
        engine::metadata_builder().build());
    const engine::test_case test_case("plain", test_program, "main",
                                      engine::metadata_builder().build());

    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::write_transaction tx = backend.start_write();
    ATF_REQUIRE_THROW(store::error, tx.put_test_case(test_case, -1));
    tx.commit();
}


ATF_TEST_CASE(put_test_case_file__empty);
ATF_TEST_CASE_HEAD(put_test_case_file__empty)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_test_case_file__empty)
{
    atf::utils::create_file("input.txt", "");

    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    backend.database().exec("PRAGMA foreign_keys = OFF");
    store::write_transaction tx = backend.start_write();
    const optional< int64_t > file_id = tx.put_test_case_file(
        "my-file", fs::path("input.txt"), 123L);
    tx.commit();
    ATF_REQUIRE(!file_id);

    sqlite::statement stmt = backend.database().create_statement(
        "SELECT * FROM test_case_files NATURAL JOIN files");
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE(put_test_case_file__some);
ATF_TEST_CASE_HEAD(put_test_case_file__some)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_test_case_file__some)
{
    const char contents[] = "This is a test!";

    atf::utils::create_file("input.txt", contents);

    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    backend.database().exec("PRAGMA foreign_keys = OFF");
    store::write_transaction tx = backend.start_write();
    const optional< int64_t > file_id = tx.put_test_case_file(
        "my-file", fs::path("input.txt"), 123L);
    tx.commit();
    ATF_REQUIRE(file_id);

    sqlite::statement stmt = backend.database().create_statement(
        "SELECT * FROM test_case_files NATURAL JOIN files");

    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(123L, stmt.safe_column_int64("test_case_id"));
    ATF_REQUIRE_EQ("my-file", stmt.safe_column_text("file_name"));
    const sqlite::blob blob = stmt.safe_column_blob("contents");
    ATF_REQUIRE(std::strlen(contents) == static_cast< std::size_t >(blob.size));
    ATF_REQUIRE(std::memcmp(contents, blob.memory, blob.size) == 0);
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE(put_test_case_file__fail);
ATF_TEST_CASE_HEAD(put_test_case_file__fail)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_test_case_file__fail)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    backend.database().exec("PRAGMA foreign_keys = OFF");
    store::write_transaction tx = backend.start_write();
    ATF_REQUIRE_THROW(store::error,
                      tx.put_test_case_file("foo", fs::path("missing"), 1L));
    tx.commit();

    sqlite::statement stmt = backend.database().create_statement(
        "SELECT * FROM test_case_files NATURAL JOIN files");
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE(put_result__ok__broken);
ATF_TEST_CASE_HEAD(put_result__ok__broken)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_result__ok__broken)
{
    const engine::test_result result(engine::test_result::broken, "a b cd");
    do_put_result_ok_test(result, "broken", "a b cd");
}


ATF_TEST_CASE(put_result__ok__expected_failure);
ATF_TEST_CASE_HEAD(put_result__ok__expected_failure)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_result__ok__expected_failure)
{
    const engine::test_result result(engine::test_result::expected_failure,
                                     "a b cd");
    do_put_result_ok_test(result, "expected_failure", "a b cd");
}


ATF_TEST_CASE(put_result__ok__failed);
ATF_TEST_CASE_HEAD(put_result__ok__failed)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_result__ok__failed)
{
    const engine::test_result result(engine::test_result::failed, "a b cd");
    do_put_result_ok_test(result, "failed", "a b cd");
}


ATF_TEST_CASE(put_result__ok__passed);
ATF_TEST_CASE_HEAD(put_result__ok__passed)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_result__ok__passed)
{
    const engine::test_result result(engine::test_result::passed);
    do_put_result_ok_test(result, "passed", NULL);
}


ATF_TEST_CASE(put_result__ok__skipped);
ATF_TEST_CASE_HEAD(put_result__ok__skipped)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_result__ok__skipped)
{
    const engine::test_result result(engine::test_result::skipped, "a b cd");
    do_put_result_ok_test(result, "skipped", "a b cd");
}


ATF_TEST_CASE(put_result__fail);
ATF_TEST_CASE_HEAD(put_result__fail)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(put_result__fail)
{
    const engine::test_result result(engine::test_result::broken, "foo");

    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::write_transaction tx = backend.start_write();
    const datetime::timestamp zero = datetime::timestamp::from_microseconds(0);
    ATF_REQUIRE_THROW(store::error, tx.put_result(result, -1, zero, zero));
    tx.commit();
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, commit__ok);
    ATF_ADD_TEST_CASE(tcs, commit__fail);
    ATF_ADD_TEST_CASE(tcs, rollback__ok);

    ATF_ADD_TEST_CASE(tcs, put_action__fail);

    ATF_ADD_TEST_CASE(tcs, put_context__fail);

    ATF_ADD_TEST_CASE(tcs, put_test_program__ok);
    ATF_ADD_TEST_CASE(tcs, put_test_program__fail);
    ATF_ADD_TEST_CASE(tcs, put_test_case__fail);
    ATF_ADD_TEST_CASE(tcs, put_test_case_file__empty);
    ATF_ADD_TEST_CASE(tcs, put_test_case_file__some);
    ATF_ADD_TEST_CASE(tcs, put_test_case_file__fail);

    ATF_ADD_TEST_CASE(tcs, put_result__ok__broken);
    ATF_ADD_TEST_CASE(tcs, put_result__ok__expected_failure);
    ATF_ADD_TEST_CASE(tcs, put_result__ok__failed);
    ATF_ADD_TEST_CASE(tcs, put_result__ok__passed);
    ATF_ADD_TEST_CASE(tcs, put_result__ok__skipped);
    ATF_ADD_TEST_CASE(tcs, put_result__fail);
}
