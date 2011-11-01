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
#include "store/backend.hpp"
#include "store/exceptions.hpp"
#include "store/transaction.hpp"
#include "utils/fs/path.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.hpp"

namespace fs = utils::fs;
namespace sqlite = utils::sqlite;


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
        tx.put(context);
        backend.database().exec(
            "CREATE TABLE foo ("
            "a REFERENCES contexts(context_id) DEFERRABLE INITIALLY DEFERRED");
        backend.database().exec("INSERT INTO foo VALUES (912378472");
        ATF_REQUIRE_THROW(store::error, tx.commit());
    }
    // If there is a problem in the handling of OIDs (such as that the OID of
    // the context has been recorded as committed above but hasn't), the
    // following would fail.  If it does not fail, then we assume all is good.
    store::transaction tx = backend.start();
    tx.put(context);
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


ATF_TEST_CASE(put__action__ok);
ATF_TEST_CASE_HEAD(put__action__ok)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(put__action__ok)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::transaction tx = backend.start();
    const engine::context context1(fs::path("/foo/bar"),
                                   std::map< std::string, std::string >());
    const engine::context context2(fs::path("/foo/baz"),
                                   std::map< std::string, std::string >());
    const engine::action action1(context1);
    const engine::action action2(context2);
    const engine::action action3(context1);
    tx.put(context1);
    tx.put(context2);
    tx.put(action1);
    tx.put(action3);
    tx.put(action2);
    tx.commit();

    // TODO(jmmv): These tests are too simplistic.  We should probably combine
    // the get/put tests, but to do so, we need to wait until the get
    // functionality is implemented.
    std::set< int > action_ids, context_ids;
    sqlite::statement stmt = backend.database().create_statement(
        "SELECT action_id,context_id FROM actions");
    while (stmt.step()) {
        action_ids.insert(stmt.column_int(0));
        context_ids.insert(stmt.column_int(1));
    }
    ATF_REQUIRE_EQ(3, action_ids.size());
    ATF_REQUIRE_EQ(2, context_ids.size());
}


ATF_TEST_CASE(put__action__fail);
ATF_TEST_CASE_HEAD(put__action__fail)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(put__action__fail)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::transaction tx = backend.start();
    const engine::context context(fs::path("/foo/bar"),
                                  std::map< std::string, std::string >());
    tx.put(context);
    const engine::action action(context);
    backend.database().exec("DROP TABLE actions");
    ATF_REQUIRE_THROW(store::error, tx.put(action));
    tx.commit();
}


ATF_TEST_CASE(put__context__ok);
ATF_TEST_CASE_HEAD(put__context__ok)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(put__context__ok)
{
    store::backend backend = store::backend::open_rw(fs::path("test.db"));
    store::transaction tx = backend.start();
    std::map< std::string, std::string > env1;
    env1["A1"] = "foo";
    env1["A2"] = "bar";
    std::map< std::string, std::string > env2;
    env2["A1"] = "not foo";
    const engine::context context1(fs::path("/foo/bar"), env1);
    const engine::context context2(fs::path("/foo/bar"), env1);
    const engine::context context3(fs::path("/foo/baz"), env2);
    tx.put(context1);
    tx.put(context3);
    tx.put(context2);
    tx.commit();

    // TODO(jmmv): These tests are too simplistic.  We should probably combine
    // the get/put tests, but to do so, we need to wait until the get
    // functionality is implemented.
    sqlite::statement stmt = backend.database().create_statement(
        "SELECT contexts.context_id, cwd, var_name, var_value "
        "FROM contexts NATURAL JOIN env_vars "
        "ORDER BY contexts.context_id, var_name");

    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ("/foo/bar", stmt.column_text(1));
    ATF_REQUIRE_EQ("A1", stmt.column_text(2));
    ATF_REQUIRE_EQ("foo", stmt.column_text(3));
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ("/foo/bar", stmt.column_text(1));
    ATF_REQUIRE_EQ("A2", stmt.column_text(2));
    ATF_REQUIRE_EQ("bar", stmt.column_text(3));

    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ("/foo/baz", stmt.column_text(1));
    ATF_REQUIRE_EQ("A1", stmt.column_text(2));
    ATF_REQUIRE_EQ("not foo", stmt.column_text(3));

    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ("/foo/bar", stmt.column_text(1));
    ATF_REQUIRE_EQ("A1", stmt.column_text(2));
    ATF_REQUIRE_EQ("foo", stmt.column_text(3));
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ("/foo/bar", stmt.column_text(1));
    ATF_REQUIRE_EQ("A2", stmt.column_text(2));
    ATF_REQUIRE_EQ("bar", stmt.column_text(3));

    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE(put__context__fail);
ATF_TEST_CASE_HEAD(put__context__fail)
{
    set_md_var("require.files", store::detail::schema_file.c_str());
}
ATF_TEST_CASE_BODY(put__context__fail)
{
    (void)store::backend::open_rw(fs::path("test.db"));
    store::backend backend = store::backend::open_ro(fs::path("test.db"));
    store::transaction tx = backend.start();
    const engine::context context(fs::path("/foo/bar"),
                                  std::map< std::string, std::string >());
    ATF_REQUIRE_THROW(store::error, tx.put(context));
    tx.commit();
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, commit__ok);
    ATF_ADD_TEST_CASE(tcs, rollback__ok);

    ATF_ADD_TEST_CASE(tcs, put__action__ok);
    ATF_ADD_TEST_CASE(tcs, put__action__fail);
    ATF_ADD_TEST_CASE(tcs, put__context__ok);
    ATF_ADD_TEST_CASE(tcs, put__context__fail);
}
