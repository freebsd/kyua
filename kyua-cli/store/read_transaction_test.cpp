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

#include "store/read_transaction.hpp"

#include <map>
#include <string>

#include <atf-c++.hpp>

#include "engine/action.hpp"
#include "engine/context.hpp"
#include "engine/test_result.hpp"
#include "store/exceptions.hpp"
#include "store/read_backend.hpp"
#include "store/write_backend.hpp"
#include "store/write_transaction.hpp"
#include "utils/datetime.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/operations.hpp"
#include "utils/optional.ipp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/statement.ipp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace logging = utils::logging;
namespace sqlite = utils::sqlite;


ATF_TEST_CASE(get_action__missing);
ATF_TEST_CASE_HEAD(get_action__missing)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_action__missing)
{
    store::write_backend::open_rw(fs::path("test.db"));  // Create database.
    store::read_backend backend = store::read_backend::open_ro(
        fs::path("test.db"));

    store::read_transaction tx = backend.start_read();
    ATF_REQUIRE_THROW_RE(store::error, "action 523: does not exist",
                         tx.get_action(523));
}


ATF_TEST_CASE(get_action__invalid_context);
ATF_TEST_CASE_HEAD(get_action__invalid_context)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_action__invalid_context)
{
    {
        store::write_backend backend = store::write_backend::open_rw(
            fs::path("test.db"));
        backend.database().exec("PRAGMA foreign_keys = OFF");
        backend.database().exec("INSERT INTO actions (action_id, context_id) "
                                "VALUES (123, 456)");
    }

    store::read_backend backend = store::read_backend::open_ro(
        fs::path("test.db"));
    store::read_transaction tx = backend.start_read();
    ATF_REQUIRE_THROW_RE(store::error, "context 456: does not exist",
                         tx.get_action(123));
}


ATF_TEST_CASE(get_action_results__none);
ATF_TEST_CASE_HEAD(get_action_results__none)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_action_results__none)
{
    store::write_backend::open_rw(fs::path("test.db"));  // Create database.
    store::read_backend backend = store::read_backend::open_ro(
        fs::path("test.db"));
    store::read_transaction tx = backend.start_read();
    store::results_iterator iter = tx.get_action_results(1);
    ATF_REQUIRE(!iter);
}


ATF_TEST_CASE(get_action_results__many);
ATF_TEST_CASE_HEAD(get_action_results__many)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_action_results__many)
{
    store::write_backend backend = store::write_backend::open_rw(
        fs::path("test.db"));

    store::write_transaction tx = backend.start_write();

    const engine::context context(fs::path("/foo/bar"),
                                  std::map< std::string, std::string >());
    const engine::action action(context);
    const int64_t context_id = tx.put_context(context);
    const int64_t action_id = tx.put_action(action, context_id);
    const int64_t action2_id = tx.put_action(action, context_id);

    const datetime::timestamp start_time1 = datetime::timestamp::from_values(
        2012, 01, 30, 22, 10, 00, 0);
    const datetime::timestamp end_time1 = datetime::timestamp::from_values(
        2012, 01, 30, 22, 15, 30, 1234);
    const datetime::timestamp start_time2 = datetime::timestamp::from_values(
        2012, 01, 30, 22, 15, 40, 987);
    const datetime::timestamp end_time2 = datetime::timestamp::from_values(
        2012, 01, 30, 22, 16, 0, 0);

    atf::utils::create_file("unused.txt", "unused file\n");

    engine::test_program test_program_1(
        "plain", fs::path("a/prog1"), fs::path("/the/root"), "suite1",
        engine::metadata_builder().build());
    engine::test_case_ptr test_case_1(new engine::test_case(
        "plain", test_program_1, "main", engine::metadata_builder().build()));
    engine::test_cases_vector test_cases_1;
    test_cases_1.push_back(test_case_1);
    test_program_1.set_test_cases(test_cases_1);
    const engine::test_result result_1(engine::test_result::passed);
    {
        const int64_t tp_id = tx.put_test_program(test_program_1, action_id);
        const int64_t tc_id = tx.put_test_case(*test_case_1, tp_id);
        atf::utils::create_file("prog1.out", "stdout of prog1\n");
        tx.put_test_case_file("__STDOUT__", fs::path("prog1.out"), tc_id);
        tx.put_test_case_file("unused.txt", fs::path("unused.txt"), tc_id);
        tx.put_result(result_1, tc_id, start_time1, end_time1);

        const int64_t tp2_id = tx.put_test_program(test_program_1, action2_id);
        const int64_t tc2_id = tx.put_test_case(*test_case_1, tp2_id);
        tx.put_test_case_file("__STDOUT__", fs::path("unused.txt"), tc2_id);
        tx.put_test_case_file("__STDERR__", fs::path("unused.txt"), tc2_id);
        tx.put_result(result_1, tc2_id, start_time1, end_time1);
    }

    engine::test_program test_program_2(
        "plain", fs::path("b/prog2"), fs::path("/the/root"), "suite2",
        engine::metadata_builder().build());
    engine::test_case_ptr test_case_2(new engine::test_case(
        "plain", test_program_2, "main", engine::metadata_builder().build()));
    engine::test_cases_vector test_cases_2;
    test_cases_2.push_back(test_case_2);
    test_program_2.set_test_cases(test_cases_2);
    const engine::test_result result_2(engine::test_result::failed,
                                       "Some text");
    {
        const int64_t tp_id = tx.put_test_program(test_program_2, action_id);
        const int64_t tc_id = tx.put_test_case(*test_case_2, tp_id);
        atf::utils::create_file("prog2.err", "stderr of prog2\n");
        tx.put_test_case_file("__STDERR__", fs::path("prog2.err"), tc_id);
        tx.put_test_case_file("unused.txt", fs::path("unused.txt"), tc_id);
        tx.put_result(result_2, tc_id, start_time2, end_time2);
    }

    tx.commit();
    backend.close();

    store::read_backend backend2 = store::read_backend::open_ro(
        fs::path("test.db"));
    store::read_transaction tx2 = backend2.start_read();
    store::results_iterator iter = tx2.get_action_results(action_id);
    ATF_REQUIRE(iter);
    ATF_REQUIRE(test_program_1 == *iter.test_program());
    ATF_REQUIRE_EQ("main", iter.test_case_name());
    ATF_REQUIRE_EQ("stdout of prog1\n", iter.stdout_contents());
    ATF_REQUIRE(iter.stderr_contents().empty());
    ATF_REQUIRE(result_1 == iter.result());
    ATF_REQUIRE(end_time1 - start_time1 == iter.duration());
    ATF_REQUIRE(++iter);
    ATF_REQUIRE(test_program_2 == *iter.test_program());
    ATF_REQUIRE_EQ("main", iter.test_case_name());
    ATF_REQUIRE(iter.stdout_contents().empty());
    ATF_REQUIRE_EQ("stderr of prog2\n", iter.stderr_contents());
    ATF_REQUIRE(result_2 == iter.result());
    ATF_REQUIRE(end_time2 - start_time2 == iter.duration());
    ATF_REQUIRE(!++iter);
}


ATF_TEST_CASE(get_latest_action__ok);
ATF_TEST_CASE_HEAD(get_latest_action__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_latest_action__ok)
{
    const engine::context context1(fs::path("/foo/bar"),
                                   std::map< std::string, std::string >());
    const engine::context context2(fs::path("/foo/baz"),
                                   std::map< std::string, std::string >());
    const engine::action exp_action1(context1);
    const engine::action exp_action2(context2);

    int64_t id2;
    {
        store::write_backend backend = store::write_backend::open_rw(
            fs::path("test.db"));
        store::write_transaction tx = backend.start_write();
        const int64_t context1_id = tx.put_context(context1);
        const int64_t context2_id = tx.put_context(context2);
        (void)tx.put_action(exp_action1, context1_id);
        id2 = tx.put_action(exp_action2, context2_id);
        tx.commit();
    }
    {
        store::read_backend backend = store::read_backend::open_ro(
            fs::path("test.db"));
        store::read_transaction tx = backend.start_read();
        const std::pair< int64_t, engine::action > latest_action =
            tx.get_latest_action();
        tx.finish();

        ATF_REQUIRE(id2 == latest_action.first);
        ATF_REQUIRE(exp_action2 == latest_action.second);
    }
}


ATF_TEST_CASE(get_latest_action__none);
ATF_TEST_CASE_HEAD(get_latest_action__none)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_latest_action__none)
{
    store::write_backend::open_rw(fs::path("test.db"));  // Create database.
    store::read_backend backend = store::read_backend::open_ro(
        fs::path("test.db"));
    store::read_transaction tx = backend.start_read();
    ATF_REQUIRE_THROW_RE(store::error, "No actions", tx.get_latest_action());
}


ATF_TEST_CASE(get_latest_action__invalid_context);
ATF_TEST_CASE_HEAD(get_latest_action__invalid_context)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_latest_action__invalid_context)
{
    {
        store::write_backend backend = store::write_backend::open_rw(
            fs::path("test.db"));
        backend.database().exec("PRAGMA foreign_keys = OFF");
        backend.database().exec("INSERT INTO actions (action_id, context_id) "
                                "VALUES (123, 456)");
    }

    store::read_backend backend = store::read_backend::open_ro(
        fs::path("test.db"));
    store::read_transaction tx = backend.start_read();
    ATF_REQUIRE_THROW_RE(store::error, "context 456: does not exist",
                         tx.get_latest_action());
}


ATF_TEST_CASE(get_context__missing);
ATF_TEST_CASE_HEAD(get_context__missing)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_context__missing)
{
    store::write_backend::open_rw(fs::path("test.db"));  // Create database.
    store::read_backend backend = store::read_backend::open_ro(
        fs::path("test.db"));

    store::read_transaction tx = backend.start_read();
    ATF_REQUIRE_THROW_RE(store::error, "context 456: does not exist",
                         tx.get_context(456));
}


ATF_TEST_CASE(get_context__invalid_cwd);
ATF_TEST_CASE_HEAD(get_context__invalid_cwd)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_context__invalid_cwd)
{
    {
        store::write_backend backend = store::write_backend::open_rw(
            fs::path("test.db"));

        sqlite::statement stmt = backend.database().create_statement(
            "INSERT INTO contexts (context_id, cwd) VALUES (78, :cwd)");
        const char buffer[10] = "foo bar";
        stmt.bind(":cwd", sqlite::blob(buffer, sizeof(buffer)));
        stmt.step_without_results();
    }

    store::read_backend backend = store::read_backend::open_ro(
        fs::path("test.db"));
    store::read_transaction tx = backend.start_read();
    ATF_REQUIRE_THROW_RE(store::error, "context 78: .*cwd.*not a string",
                         tx.get_context(78));
}


ATF_TEST_CASE(get_context__invalid_env_vars);
ATF_TEST_CASE_HEAD(get_context__invalid_env_vars)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_context__invalid_env_vars)
{
    store::write_backend backend = store::write_backend::open_rw(
        fs::path("test.db"));

    backend.database().exec("INSERT INTO contexts (context_id, cwd) "
                            "VALUES (10, '/foo/bar')");
    backend.database().exec("INSERT INTO contexts (context_id, cwd) "
                            "VALUES (20, '/foo/bar')");

    const char buffer[10] = "foo bar";

    {
        sqlite::statement stmt = backend.database().create_statement(
            "INSERT INTO env_vars (context_id, var_name, var_value) "
            "VALUES (10, :var_name, 'abc')");
        stmt.bind(":var_name", sqlite::blob(buffer, sizeof(buffer)));
        stmt.step_without_results();
    }

    {
        sqlite::statement stmt = backend.database().create_statement(
            "INSERT INTO env_vars (context_id, var_name, var_value) "
            "VALUES (20, 'abc', :var_value)");
        stmt.bind(":var_value", sqlite::blob(buffer, sizeof(buffer)));
        stmt.step_without_results();
    }

    backend.close();

    store::read_backend backend2 = store::read_backend::open_ro(
        fs::path("test.db"));
    store::read_transaction tx = backend2.start_read();
    ATF_REQUIRE_THROW_RE(store::error, "context 10: .*var_name.*not a string",
                         tx.get_context(10));
    ATF_REQUIRE_THROW_RE(store::error, "context 20: .*var_value.*not a string",
                         tx.get_context(20));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, get_action__missing);
    ATF_ADD_TEST_CASE(tcs, get_action__invalid_context);

    ATF_ADD_TEST_CASE(tcs, get_action_results__none);
    ATF_ADD_TEST_CASE(tcs, get_action_results__many);

    ATF_ADD_TEST_CASE(tcs, get_latest_action__ok);
    ATF_ADD_TEST_CASE(tcs, get_latest_action__none);
    ATF_ADD_TEST_CASE(tcs, get_latest_action__invalid_context);

    ATF_ADD_TEST_CASE(tcs, get_context__missing);
    ATF_ADD_TEST_CASE(tcs, get_context__invalid_cwd);
    ATF_ADD_TEST_CASE(tcs, get_context__invalid_env_vars);
}
