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

#include <map>
#include <set>
#include <string>

#include <atf-c++.hpp>

#include "engine/action.hpp"
#include "engine/context.hpp"
#include "engine/plain_iface/test_case.hpp"
#include "engine/plain_iface/test_program.hpp"
#include "engine/results.hpp"
#include "store/backend.hpp"
#include "store/exceptions.hpp"
#include "store/transaction.hpp"
#include "utils/datetime.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace plain_iface = engine::plain_iface;
namespace results = engine::results;
namespace sqlite = utils::sqlite;

using utils::none;


namespace {


/// Performs a test for a working put_result
///
/// \param result The result object to put.
/// \param result_type The textual name of the result to expect in the
///     database.
/// \param exp_reason The reason to expect in the database.  This is separate
///     from the result parameter so that we can handle passed() here as well.
///     Just provide NULL in this case.
template< class Result >
static void
do_put_result_ok_test(const Result& result, const char* result_type,
                      const char* exp_reason)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    backend.database().exec("PRAGMA foreign_keys = OFF");
    store::transaction tx = backend.start();
    tx.put_result(results::result_ptr(new Result(result)), 312);
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
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(commit__ok)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::transaction tx = backend.start();
    backend.database().exec("CREATE TABLE a (b INTEGER PRIMARY KEY)");
    backend.database().exec("SELECT * FROM a");
    tx.commit();
    backend.database().exec("SELECT * FROM a");
}


ATF_TEST_CASE(commit__fail);
ATF_TEST_CASE_HEAD(commit__fail)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(commit__fail)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    const engine::context context(fs::path("/foo/bar"),
                                  std::map< std::string, std::string >());
    {
        store::transaction tx = backend.start();
        tx.put_context(context);
        backend.database().exec(
            "CREATE TABLE foo ("
            "a REFERENCES contexts(context_id) DEFERRABLE INITIALLY DEFERRED");
        backend.database().exec("INSERT INTO foo VALUES (912378472");
        ATF_REQUIRE_THROW(store::error, tx.commit());
    }
    // If the code attempts to maintain any state regarding the already-put
    // objects and the commit does not clean up correctly, this would fail in
    // some manner.
    store::transaction tx = backend.start();
    tx.put_context(context);
    tx.commit();
}


ATF_TEST_CASE(rollback__ok);
ATF_TEST_CASE_HEAD(rollback__ok)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(rollback__ok)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::transaction tx = backend.start();
    backend.database().exec("CREATE TABLE a_table (b INTEGER PRIMARY KEY)");
    backend.database().exec("SELECT * FROM a_table");
    tx.rollback();
    ATF_REQUIRE_THROW_RE(sqlite::error, "a_table",
                         backend.database().exec("SELECT * FROM a_table"));
}


ATF_TEST_CASE(get_put_action__ok);
ATF_TEST_CASE_HEAD(get_put_action__ok)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(get_put_action__ok)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    const engine::context context1(fs::path("/foo/bar"),
                                   std::map< std::string, std::string >());
    const engine::context context2(fs::path("/foo/baz"),
                                   std::map< std::string, std::string >());
    const engine::action exp_action1(context1);
    const engine::action exp_action2(context2);
    const engine::action exp_action3(context1);

    int64_t id1, id2, id3;
    {
        store::transaction tx = backend.start();
        const int64_t context1_id = tx.put_context(context1);
        const int64_t context2_id = tx.put_context(context2);
        id1 = tx.put_action(exp_action1, context1_id);
        id3 = tx.put_action(exp_action3, context1_id);
        id2 = tx.put_action(exp_action2, context2_id);
        tx.commit();
    }
    {
        store::transaction tx = backend.start();
        const engine::action action1 = tx.get_action(id1);
        const engine::action action2 = tx.get_action(id2);
        const engine::action action3 = tx.get_action(id3);
        tx.rollback();

        ATF_REQUIRE(exp_action1 == action1);
        ATF_REQUIRE(exp_action2 == action2);
        ATF_REQUIRE(exp_action3 == action3);
    }
}


ATF_TEST_CASE(get_put_action__get_fail__missing);
ATF_TEST_CASE_HEAD(get_put_action__get_fail__missing)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(get_put_action__get_fail__missing)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));

    store::transaction tx = backend.start();
    ATF_REQUIRE_THROW_RE(store::error, "action 523: does not exist",
                         tx.get_action(523));
}


ATF_TEST_CASE(get_put_action__get_fail__invalid_context);
ATF_TEST_CASE_HEAD(get_put_action__get_fail__invalid_context)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(get_put_action__get_fail__invalid_context)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    backend.database().exec("PRAGMA foreign_keys = OFF");
    backend.database().exec("INSERT INTO actions (action_id, context_id) "
                            "VALUES (123, 456)");

    store::transaction tx = backend.start();
    ATF_REQUIRE_THROW_RE(store::error, "context 456: does not exist",
                         tx.get_action(123));
}


ATF_TEST_CASE(get_put_action__put_fail);
ATF_TEST_CASE_HEAD(get_put_action__put_fail)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(get_put_action__put_fail)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::transaction tx = backend.start();
    const engine::context context(fs::path("/foo/bar"),
                                  std::map< std::string, std::string >());
    const int64_t context_id = tx.put_context(context);
    const engine::action action(context);
    backend.database().exec("DROP TABLE actions");
    ATF_REQUIRE_THROW(store::error, tx.put_action(action, context_id));
    tx.commit();
}


ATF_TEST_CASE(get_put_context__ok);
ATF_TEST_CASE_HEAD(get_put_context__ok)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(get_put_context__ok)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));

    std::map< std::string, std::string > env1;
    env1["A1"] = "foo";
    env1["A2"] = "bar";
    std::map< std::string, std::string > env2;
    const engine::context exp_context1(fs::path("/foo/bar"), env1);
    const engine::context exp_context2(fs::path("/foo/bar"), env1);
    const engine::context exp_context3(fs::path("/foo/baz"), env2);

    int64_t id1, id2, id3;
    {
        store::transaction tx = backend.start();
        id1 = tx.put_context(exp_context1);
        id3 = tx.put_context(exp_context3);
        id2 = tx.put_context(exp_context2);
        tx.commit();
    }
    {
        store::transaction tx = backend.start();
        const engine::context context1 = tx.get_context(id1);
        const engine::context context2 = tx.get_context(id2);
        const engine::context context3 = tx.get_context(id3);
        tx.rollback();

        ATF_REQUIRE(exp_context1 == context1);
        ATF_REQUIRE(exp_context2 == context2);
        ATF_REQUIRE(exp_context3 == context3);
    }
}


ATF_TEST_CASE(get_put_context__get_fail__missing);
ATF_TEST_CASE_HEAD(get_put_context__get_fail__missing)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(get_put_context__get_fail__missing)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));

    store::transaction tx = backend.start();
    ATF_REQUIRE_THROW_RE(store::error, "context 456: does not exist",
                         tx.get_context(456));
}


ATF_TEST_CASE(get_put_context__get_fail__invalid_cwd);
ATF_TEST_CASE_HEAD(get_put_context__get_fail__invalid_cwd)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(get_put_context__get_fail__invalid_cwd)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));

    sqlite::statement stmt = backend.database().create_statement(
        "INSERT INTO contexts (context_id, cwd) VALUES (78, :cwd)");
    const char buffer[10] = "foo bar";
    stmt.bind_blob(":cwd", buffer, sizeof(buffer));
    stmt.step_without_results();

    store::transaction tx = backend.start();
    ATF_REQUIRE_THROW_RE(store::error, "context 78: .*cwd.*not a string",
                         tx.get_context(78));
}


ATF_TEST_CASE(get_put_context__get_fail__invalid_env_vars);
ATF_TEST_CASE_HEAD(get_put_context__get_fail__invalid_env_vars)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(get_put_context__get_fail__invalid_env_vars)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));

    backend.database().exec("INSERT INTO contexts (context_id, cwd) "
                            "VALUES (10, '/foo/bar')");
    backend.database().exec("INSERT INTO contexts (context_id, cwd) "
                            "VALUES (20, '/foo/bar')");

    const char buffer[10] = "foo bar";

    {
        sqlite::statement stmt = backend.database().create_statement(
            "INSERT INTO env_vars (context_id, var_name, var_value) "
            "VALUES (10, :var_name, 'abc')");
        stmt.bind_blob(":var_name", buffer, sizeof(buffer));
        stmt.step_without_results();
    }

    {
        sqlite::statement stmt = backend.database().create_statement(
            "INSERT INTO env_vars (context_id, var_name, var_value) "
            "VALUES (20, 'abc', :var_value)");
        stmt.bind_blob(":var_value", buffer, sizeof(buffer));
        stmt.step_without_results();
    }

    store::transaction tx = backend.start();
    ATF_REQUIRE_THROW_RE(store::error, "context 10: .*var_name.*not a string",
                         tx.get_context(10));
    ATF_REQUIRE_THROW_RE(store::error, "context 20: .*var_value.*not a string",
                         tx.get_context(20));
}


ATF_TEST_CASE(get_put_context__put_fail);
ATF_TEST_CASE_HEAD(get_put_context__put_fail)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(get_put_context__put_fail)
{
    (void)store::backend::open_rw(fs::path("test.db"));
    store::backend backend = store::backend::open_ro(fs::path("test.db"));
    store::transaction tx = backend.start();
    const engine::context context(fs::path("/foo/bar"),
                                  std::map< std::string, std::string >());
    ATF_REQUIRE_THROW(store::error, tx.put_context(context));
    tx.commit();
}


ATF_TEST_CASE(put_test_program__ok);
ATF_TEST_CASE_HEAD(put_test_program__ok)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(put_test_program__ok)
{
    // TODO(jmmv): Use a mock test program.
    const plain_iface::test_program test_program(
        fs::path("the/binary"), fs::path("/some/root"), "the-suite", none);

    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    backend.database().exec("PRAGMA foreign_keys = OFF");
    store::transaction tx = backend.start();
    const int64_t test_program_id = tx.put_test_program(test_program, 15);
    tx.commit();

    sqlite::statement stmt = backend.database().create_statement(
        "SELECT test_program_id, action_id, binary_path, test_suite_name "
        "FROM test_programs");

    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(test_program_id, stmt.column_int64(0));
    ATF_REQUIRE_EQ(15, stmt.column_int64(1));
    ATF_REQUIRE_EQ("/some/root/the/binary", stmt.column_text(2));
    ATF_REQUIRE_EQ("the-suite", stmt.column_text(3));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE(put_test_program__fail);
ATF_TEST_CASE_HEAD(put_test_program__fail)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(put_test_program__fail)
{
    // TODO(jmmv): Use a mock test program.
    const plain_iface::test_program test_program(
        fs::path("the/binary"), fs::path("/some/root"), "the-suite", none);

    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::transaction tx = backend.start();
    ATF_REQUIRE_THROW(store::error, tx.put_test_program(test_program, -1));
    tx.commit();
}


ATF_TEST_CASE(put_test_case__ok);
ATF_TEST_CASE_HEAD(put_test_case__ok)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(put_test_case__ok)
{
    // TODO(jmmv): Use a mock test program and test case.
    const plain_iface::test_program test_program(
        fs::path("the/binary"), fs::path("/some/root"), "the-suite",
        utils::make_optional(datetime::delta(512, 0)));
    const plain_iface::test_case test_case(test_program);

    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    backend.database().exec("PRAGMA foreign_keys = OFF");
    store::transaction tx = backend.start();
    const int64_t test_program_id = tx.put_test_program(test_program, 15);
    const int64_t test_case_id = tx.put_test_case(test_case, test_program_id);
    tx.commit();

    {
        sqlite::statement stmt = backend.database().create_statement(
            "SELECT test_case_id, test_program_id, name "
            "FROM test_cases");

        ATF_REQUIRE(stmt.step());
        ATF_REQUIRE_EQ(test_case_id, stmt.column_int64(0));
        ATF_REQUIRE_EQ(test_program_id, stmt.column_int64(1));
        ATF_REQUIRE_EQ("main", stmt.column_text(2));
        ATF_REQUIRE(!stmt.step());
    }

    {
        sqlite::statement stmt = backend.database().create_statement(
            "SELECT test_case_id, var_name, var_value "
            "FROM test_cases_metadata");

        ATF_REQUIRE(stmt.step());
        ATF_REQUIRE_EQ(test_case_id, stmt.column_int64(0));
        ATF_REQUIRE_EQ("timeout", stmt.column_text(1));
        ATF_REQUIRE_EQ("512", stmt.column_text(2));
        ATF_REQUIRE(!stmt.step());
    }
}


ATF_TEST_CASE(put_test_case__fail);
ATF_TEST_CASE_HEAD(put_test_case__fail)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(put_test_case__fail)
{
    // TODO(jmmv): Use a mock test program and test case.
    const plain_iface::test_program test_program(
        fs::path("the/binary"), fs::path("/some/root"), "the-suite", none);
    const plain_iface::test_case test_case(test_program);

    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::transaction tx = backend.start();
    ATF_REQUIRE_THROW(store::error, tx.put_test_case(test_case, -1));
    tx.commit();
}


ATF_TEST_CASE(put_result__ok__broken);
ATF_TEST_CASE_HEAD(put_result__ok__broken)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(put_result__ok__broken)
{
    do_put_result_ok_test(results::broken("a b cd"), "broken", "a b cd");
}


ATF_TEST_CASE(put_result__ok__expected_failure);
ATF_TEST_CASE_HEAD(put_result__ok__expected_failure)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(put_result__ok__expected_failure)
{
    do_put_result_ok_test(results::expected_failure("a b cd"),
                          "expected_failure", "a b cd");
}


ATF_TEST_CASE(put_result__ok__failed);
ATF_TEST_CASE_HEAD(put_result__ok__failed)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(put_result__ok__failed)
{
    do_put_result_ok_test(results::failed("a b cd"), "failed", "a b cd");
}


ATF_TEST_CASE(put_result__ok__passed);
ATF_TEST_CASE_HEAD(put_result__ok__passed)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(put_result__ok__passed)
{
    do_put_result_ok_test(results::passed(), "passed", NULL);
}


ATF_TEST_CASE(put_result__ok__skipped);
ATF_TEST_CASE_HEAD(put_result__ok__skipped)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(put_result__ok__skipped)
{
    do_put_result_ok_test(results::skipped("a b cd"), "skipped", "a b cd");
}


ATF_TEST_CASE(put_result__fail);
ATF_TEST_CASE_HEAD(put_result__fail)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(put_result__fail)
{
    const results::result_ptr result(new results::broken("foo"));

    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::transaction tx = backend.start();
    ATF_REQUIRE_THROW(store::error, tx.put_result(result, -1));
    tx.commit();
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, commit__ok);
    ATF_ADD_TEST_CASE(tcs, rollback__ok);

    ATF_ADD_TEST_CASE(tcs, get_put_action__ok);
    ATF_ADD_TEST_CASE(tcs, get_put_action__get_fail__missing);
    ATF_ADD_TEST_CASE(tcs, get_put_action__get_fail__invalid_context);
    ATF_ADD_TEST_CASE(tcs, get_put_action__put_fail);

    ATF_ADD_TEST_CASE(tcs, get_put_context__ok);
    ATF_ADD_TEST_CASE(tcs, get_put_context__get_fail__missing);
    ATF_ADD_TEST_CASE(tcs, get_put_context__get_fail__invalid_cwd);
    ATF_ADD_TEST_CASE(tcs, get_put_context__get_fail__invalid_env_vars);
    ATF_ADD_TEST_CASE(tcs, get_put_context__put_fail);

    ATF_ADD_TEST_CASE(tcs, put_test_program__ok);
    ATF_ADD_TEST_CASE(tcs, put_test_program__fail);
    ATF_ADD_TEST_CASE(tcs, put_test_case__ok);
    ATF_ADD_TEST_CASE(tcs, put_test_case__fail);

    ATF_ADD_TEST_CASE(tcs, put_result__ok__broken);
    ATF_ADD_TEST_CASE(tcs, put_result__ok__expected_failure);
    ATF_ADD_TEST_CASE(tcs, put_result__ok__failed);
    ATF_ADD_TEST_CASE(tcs, put_result__ok__passed);
    ATF_ADD_TEST_CASE(tcs, put_result__ok__skipped);
    ATF_ADD_TEST_CASE(tcs, put_result__fail);
}
