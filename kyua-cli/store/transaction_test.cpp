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
#include <string>

#include <atf-c++.hpp>

#include "engine/action.hpp"
#include "engine/context.hpp"
#include "store/read_backend.hpp"
#include "store/read_transaction.hpp"
#include "store/write_backend.hpp"
#include "store/write_transaction.hpp"
#include "utils/datetime.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/operations.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/units.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace logging = utils::logging;
namespace units = utils::units;


ATF_TEST_CASE(get_put_action__ok);
ATF_TEST_CASE_HEAD(get_put_action__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_put_action__ok)
{
    const engine::context context1(fs::path("/foo/bar"),
                                   std::map< std::string, std::string >());
    const engine::context context2(fs::path("/foo/baz"),
                                   std::map< std::string, std::string >());
    const engine::action exp_action1(context1);
    const engine::action exp_action2(context2);
    const engine::action exp_action3(context1);

    int64_t id1, id2, id3;
    {
        store::write_backend backend = store::write_backend::open_rw(
            fs::path("test.db"));
        store::write_transaction tx = backend.start_write();
        const int64_t context1_id = tx.put_context(context1);
        const int64_t context2_id = tx.put_context(context2);
        id1 = tx.put_action(exp_action1, context1_id);
        id3 = tx.put_action(exp_action3, context1_id);
        id2 = tx.put_action(exp_action2, context2_id);
        tx.commit();
    }
    {
        store::read_backend backend = store::read_backend::open_ro(
            fs::path("test.db"));
        store::read_transaction tx = backend.start_read();
        const engine::action action1 = tx.get_action(id1);
        const engine::action action2 = tx.get_action(id2);
        const engine::action action3 = tx.get_action(id3);
        tx.finish();

        ATF_REQUIRE(exp_action1 == action1);
        ATF_REQUIRE(exp_action2 == action2);
        ATF_REQUIRE(exp_action3 == action3);
    }
}


ATF_TEST_CASE(get_put_context__ok);
ATF_TEST_CASE_HEAD(get_put_context__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_put_context__ok)
{
    std::map< std::string, std::string > env1;
    env1["A1"] = "foo";
    env1["A2"] = "bar";
    std::map< std::string, std::string > env2;
    const engine::context exp_context1(fs::path("/foo/bar"), env1);
    const engine::context exp_context2(fs::path("/foo/bar"), env1);
    const engine::context exp_context3(fs::path("/foo/baz"), env2);

    int64_t id1, id2, id3;
    {
        store::write_backend backend = store::write_backend::open_rw(
            fs::path("test.db"));
        store::write_transaction tx = backend.start_write();
        id1 = tx.put_context(exp_context1);
        id3 = tx.put_context(exp_context3);
        id2 = tx.put_context(exp_context2);
        tx.commit();
    }
    {
        store::read_backend backend = store::read_backend::open_ro(
            fs::path("test.db"));
        store::read_transaction tx = backend.start_read();
        const engine::context context1 = tx.get_context(id1);
        const engine::context context2 = tx.get_context(id2);
        const engine::context context3 = tx.get_context(id3);
        tx.finish();

        ATF_REQUIRE(exp_context1 == context1);
        ATF_REQUIRE(exp_context2 == context2);
        ATF_REQUIRE(exp_context3 == context3);
    }
}


ATF_TEST_CASE(get_put_test_case__ok);
ATF_TEST_CASE_HEAD(get_put_test_case__ok)
{
    logging::set_inmemory();
    set_md_var("require.files", store::detail::schema_file().c_str());
}
ATF_TEST_CASE_BODY(get_put_test_case__ok)
{
    engine::test_program test_program(
        "atf", fs::path("the/binary"), fs::path("/some/root"), "the-suite",
        engine::metadata_builder().build());

    const engine::test_case_ptr test_case1(new engine::test_case(
        "atf", test_program, "tc1", engine::metadata_builder().build()));

    const engine::metadata md2 = engine::metadata_builder()
        .add_allowed_architecture("powerpc")
        .add_allowed_architecture("x86_64")
        .add_allowed_platform("amd64")
        .add_allowed_platform("macppc")
        .add_custom("X-user1", "value1")
        .add_custom("X-user2", "value2")
        .add_required_config("var1")
        .add_required_config("var2")
        .add_required_config("var3")
        .add_required_file(fs::path("/file1/yes"))
        .add_required_file(fs::path("/file2/foo"))
        .add_required_program(fs::path("/bin/ls"))
        .add_required_program(fs::path("cp"))
        .set_description("The description")
        .set_has_cleanup(true)
        .set_required_memory(units::bytes::parse("1k"))
        .set_required_user("root")
        .set_timeout(datetime::delta(520, 0))
        .build();
    const engine::test_case_ptr test_case2(new engine::test_case(
        "atf", test_program, "tc2", md2));

    {
        engine::test_cases_vector test_cases;
        test_cases.push_back(test_case1);
        test_cases.push_back(test_case2);
        test_program.set_test_cases(test_cases);
    }

    int64_t test_program_id;
    {
        store::write_backend backend = store::write_backend::open_rw(
            fs::path("test.db"));
        backend.database().exec("PRAGMA foreign_keys = OFF");

        store::write_transaction tx = backend.start_write();
        test_program_id = tx.put_test_program(test_program, 15);
        tx.put_test_case(*test_case1, test_program_id);
        tx.put_test_case(*test_case2, test_program_id);
        tx.commit();
    }

    store::read_backend backend = store::read_backend::open_ro(
        fs::path("test.db"));
    backend.database().exec("PRAGMA foreign_keys = OFF");

    store::read_transaction tx = backend.start_read();
    const engine::test_program_ptr loaded_test_program =
        store::detail::get_test_program(backend, test_program_id);
    ATF_REQUIRE(test_program == *loaded_test_program);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, get_put_action__ok);

    ATF_ADD_TEST_CASE(tcs, get_put_context__ok);

    ATF_ADD_TEST_CASE(tcs, get_put_test_case__ok);
}
