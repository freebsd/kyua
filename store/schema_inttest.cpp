// Copyright 2013 The Kyua Authors.
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

#include <atf-c++.hpp>

#include "model/context.hpp"
#include "model/metadata.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "store/exceptions.hpp"
#include "store/metadata.hpp"
#include "store/migrate.hpp"
#include "store/read_backend.hpp"
#include "store/read_transaction.hpp"
#include "store/write_backend.hpp"
#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/operations.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/statement.hpp"
#include "utils/stream.hpp"
#include "utils/units.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace logging = utils::logging;
namespace sqlite = utils::sqlite;
namespace units = utils::units;


namespace {


/// Gets a data file from the tests directory.
///
/// We cannot use the srcdir property because the required files are not there
/// when building with an object directory.  In those cases, the data files
/// remainin the source directory while the resulting test program is in the
/// object directory, thus having the wrong value for its srcdir property.
///
/// \param name Basename of the test data file to query.
///
/// \return The actual path to the requested data file.
static fs::path
testdata_file(const std::string& name)
{
    const fs::path testdatadir(utils::getenv_with_default(
        "KYUA_STORETESTDATADIR", KYUA_STORETESTDATADIR));
    return testdatadir / name;
}


/// Validates the contents of the action with identifier 1.
///
/// \param dbpath Path to the database in which to check the action contents.
static void
check_action_1(const fs::path& dbpath)
{
    store::read_backend backend = store::read_backend::open_ro(dbpath);
    store::read_transaction transaction = backend.start_read();

    const fs::path root("/some/root");
    std::map< std::string, std::string > environment;
    const model::context context(root, environment);

    ATF_REQUIRE_EQ(context, transaction.get_context());

    store::results_iterator iter = transaction.get_results();
    ATF_REQUIRE(!iter);
}


/// Validates the contents of the action with identifier 2.
///
/// \param dbpath Path to the database in which to check the action contents.
static void
check_action_2(const fs::path& dbpath)
{
    store::read_backend backend = store::read_backend::open_ro(dbpath);
    store::read_transaction transaction = backend.start_read();

    const fs::path root("/test/suite/root");
    std::map< std::string, std::string > environment;
    environment["HOME"] = "/home/test";
    environment["PATH"] = "/bin:/usr/bin";
    const model::context context(root, environment);

    ATF_REQUIRE_EQ(context, transaction.get_context());

    const model::test_program test_program_1 = model::test_program_builder(
        "plain", fs::path("foo_test"), fs::path("/test/suite/root"),
        "suite-name")
        .add_test_case("main")
        .build();
    const model::test_result result_1(model::test_result_passed);

    const model::test_program test_program_2 = model::test_program_builder(
        "plain", fs::path("subdir/another_test"), fs::path("/test/suite/root"),
        "subsuite-name")
        .add_test_case("main",
                       model::metadata_builder()
                       .set_timeout(datetime::delta(10, 0))
                       .build())
        .set_metadata(model::metadata_builder()
                      .set_timeout(datetime::delta(10, 0))
                      .build())
        .build();
    const model::test_result result_2(model::test_result_failed,
                                      "Exited with code 1");

    const model::test_program test_program_3 = model::test_program_builder(
        "plain", fs::path("subdir/bar_test"), fs::path("/test/suite/root"),
        "subsuite-name")
        .add_test_case("main")
        .build();
    const model::test_result result_3(model::test_result_broken,
                                      "Received signal 1");

    const model::test_program test_program_4 = model::test_program_builder(
        "plain", fs::path("top_test"), fs::path("/test/suite/root"),
        "suite-name")
        .add_test_case("main")
        .build();
    const model::test_result result_4(model::test_result_expected_failure,
                                      "Known bug");

    const model::test_program test_program_5 = model::test_program_builder(
        "plain", fs::path("last_test"), fs::path("/test/suite/root"),
        "suite-name")
        .add_test_case("main")
        .build();
    const model::test_result result_5(model::test_result_skipped,
                                      "Does not apply");

    store::results_iterator iter = transaction.get_results();
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_1, *iter.test_program());
    ATF_REQUIRE_EQ("main", iter.test_case_name());
    ATF_REQUIRE_EQ(result_1, iter.result());
    ATF_REQUIRE(iter.stdout_contents().empty());
    ATF_REQUIRE(iter.stderr_contents().empty());
    ATF_REQUIRE_EQ(1357643611000000LL, iter.start_time().to_microseconds());
    ATF_REQUIRE_EQ(1357643621000500LL, iter.end_time().to_microseconds());
    ATF_REQUIRE_EQ(datetime::delta(10, 500), iter.elapsed_time());

    ++iter;
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_5, *iter.test_program());
    ATF_REQUIRE_EQ("main", iter.test_case_name());
    ATF_REQUIRE_EQ(result_5, iter.result());
    ATF_REQUIRE(iter.stdout_contents().empty());
    ATF_REQUIRE(iter.stderr_contents().empty());
    ATF_REQUIRE_EQ(1357643632000000LL, iter.start_time().to_microseconds());
    ATF_REQUIRE_EQ(1357643638000000LL, iter.end_time().to_microseconds());
    ATF_REQUIRE_EQ(datetime::delta(6, 0), iter.elapsed_time());

    ++iter;
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_2, *iter.test_program());
    ATF_REQUIRE_EQ("main", iter.test_case_name());
    ATF_REQUIRE_EQ(result_2, iter.result());
    ATF_REQUIRE_EQ("Test stdout", iter.stdout_contents());
    ATF_REQUIRE_EQ("Test stderr", iter.stderr_contents());
    ATF_REQUIRE_EQ(1357643622001200LL, iter.start_time().to_microseconds());
    ATF_REQUIRE_EQ(1357643622900021LL, iter.end_time().to_microseconds());
    ATF_REQUIRE_EQ(datetime::delta(0, 898821), iter.elapsed_time());

    ++iter;
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_3, *iter.test_program());
    ATF_REQUIRE_EQ("main", iter.test_case_name());
    ATF_REQUIRE_EQ(result_3, iter.result());
    ATF_REQUIRE(iter.stdout_contents().empty());
    ATF_REQUIRE(iter.stderr_contents().empty());
    ATF_REQUIRE_EQ(1357643623500000LL, iter.start_time().to_microseconds());
    ATF_REQUIRE_EQ(1357643630981932LL, iter.end_time().to_microseconds());
    ATF_REQUIRE_EQ(datetime::delta(7, 481932), iter.elapsed_time());

    ++iter;
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_4, *iter.test_program());
    ATF_REQUIRE_EQ("main", iter.test_case_name());
    ATF_REQUIRE_EQ(result_4, iter.result());
    ATF_REQUIRE(iter.stdout_contents().empty());
    ATF_REQUIRE(iter.stderr_contents().empty());
    ATF_REQUIRE_EQ(1357643631000000LL, iter.start_time().to_microseconds());
    ATF_REQUIRE_EQ(1357643631020000LL, iter.end_time().to_microseconds());
    ATF_REQUIRE_EQ(datetime::delta(0, 20000), iter.elapsed_time());

    ++iter;
    ATF_REQUIRE(!iter);
}


/// Validates the contents of the action with identifier 3.
///
/// \param dbpath Path to the database in which to check the action contents.
static void
check_action_3(const fs::path& dbpath)
{
    store::read_backend backend = store::read_backend::open_ro(dbpath);
    store::read_transaction transaction = backend.start_read();

    const fs::path root("/usr/tests");
    std::map< std::string, std::string > environment;
    environment["PATH"] = "/bin:/usr/bin";
    const model::context context(root, environment);

    ATF_REQUIRE_EQ(context, transaction.get_context());

    const model::test_program test_program_6 = model::test_program_builder(
        "atf", fs::path("complex_test"), fs::path("/usr/tests"),
        "suite-name")
        .add_test_case("this_passes")
        .add_test_case("this_fails",
                       model::metadata_builder()
                       .set_description("Test description")
                       .set_has_cleanup(true)
                       .set_required_memory(units::bytes(128))
                       .set_required_user("root")
                       .build())
        .add_test_case("this_skips",
                       model::metadata_builder()
                       .add_allowed_architecture("powerpc")
                       .add_allowed_architecture("x86_64")
                       .add_allowed_platform("amd64")
                       .add_allowed_platform("macppc")
                       .add_required_config("X-foo")
                       .add_required_config("unprivileged_user")
                       .add_required_file(fs::path("/the/data/file"))
                       .add_required_program(fs::path("/bin/ls"))
                       .add_required_program(fs::path("cp"))
                       .set_description("Test explanation")
                       .set_has_cleanup(true)
                       .set_required_memory(units::bytes(512))
                       .set_required_user("unprivileged")
                       .set_timeout(datetime::delta(600, 0))
                       .build())
        .build();
    const model::test_result result_6(model::test_result_passed);
    const model::test_result result_7(model::test_result_failed,
                                      "Some reason");
    const model::test_result result_8(model::test_result_skipped,
                                      "Another reason");

    const model::test_program test_program_7 = model::test_program_builder(
        "atf", fs::path("simple_test"), fs::path("/usr/tests"),
        "subsuite-name")
        .add_test_case("main",
                       model::metadata_builder()
                       .set_description("More text")
                       .set_has_cleanup(true)
                       .set_required_memory(units::bytes(128))
                       .set_required_user("unprivileged")
                       .build())
        .build();
    const model::test_result result_9(model::test_result_failed,
                                      "Exited with code 1");

    store::results_iterator iter = transaction.get_results();
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_6, *iter.test_program());
    ATF_REQUIRE_EQ("this_fails", iter.test_case_name());
    ATF_REQUIRE_EQ(result_7, iter.result());
    ATF_REQUIRE(iter.stdout_contents().empty());
    ATF_REQUIRE(iter.stderr_contents().empty());
    ATF_REQUIRE_EQ(1357648719000000LL, iter.start_time().to_microseconds());
    ATF_REQUIRE_EQ(1357648720897182LL, iter.end_time().to_microseconds());
    ATF_REQUIRE_EQ(datetime::delta(1, 897182), iter.elapsed_time());

    ++iter;
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_6, *iter.test_program());
    ATF_REQUIRE_EQ("this_passes", iter.test_case_name());
    ATF_REQUIRE_EQ(result_6, iter.result());
    ATF_REQUIRE(iter.stdout_contents().empty());
    ATF_REQUIRE(iter.stderr_contents().empty());
    ATF_REQUIRE_EQ(1357648712000000LL, iter.start_time().to_microseconds());
    ATF_REQUIRE_EQ(1357648718000000LL, iter.end_time().to_microseconds());
    ATF_REQUIRE_EQ(datetime::delta(6, 0), iter.elapsed_time());

    ++iter;
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_6, *iter.test_program());
    ATF_REQUIRE_EQ("this_skips", iter.test_case_name());
    ATF_REQUIRE_EQ(result_8, iter.result());
    ATF_REQUIRE_EQ("Another stdout", iter.stdout_contents());
    ATF_REQUIRE(iter.stderr_contents().empty());
    ATF_REQUIRE_EQ(1357648729182013LL, iter.start_time().to_microseconds());
    ATF_REQUIRE_EQ(1357648730000000LL, iter.end_time().to_microseconds());
    ATF_REQUIRE_EQ(datetime::delta(0, 817987), iter.elapsed_time());

    ++iter;
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_7, *iter.test_program());
    ATF_REQUIRE_EQ("main", iter.test_case_name());
    ATF_REQUIRE_EQ(result_9, iter.result());
    ATF_REQUIRE(iter.stdout_contents().empty());
    ATF_REQUIRE_EQ("Another stderr", iter.stderr_contents());
    ATF_REQUIRE_EQ(1357648740120000LL, iter.start_time().to_microseconds());
    ATF_REQUIRE_EQ(1357648750081700LL, iter.end_time().to_microseconds());
    ATF_REQUIRE_EQ(datetime::delta(9, 961700), iter.elapsed_time());

    ++iter;
    ATF_REQUIRE(!iter);
}


/// Validates the contents of the action with identifier 4.
///
/// \param dbpath Path to the database in which to check the action contents.
static void
check_action_4(const fs::path& dbpath)
{
    store::read_backend backend = store::read_backend::open_ro(dbpath);
    store::read_transaction transaction = backend.start_read();

    const fs::path root("/usr/tests");
    std::map< std::string, std::string > environment;
    environment["LANG"] = "C";
    environment["PATH"] = "/bin:/usr/bin";
    environment["TERM"] = "xterm";
    const model::context context(root, environment);

    ATF_REQUIRE_EQ(context, transaction.get_context());

    const model::test_program test_program_8 = model::test_program_builder(
        "plain", fs::path("subdir/another_test"), fs::path("/usr/tests"),
        "subsuite-name")
        .add_test_case("main",
                       model::metadata_builder()
                       .set_timeout(datetime::delta(10, 0))
                       .build())
        .set_metadata(model::metadata_builder()
                      .set_timeout(datetime::delta(10, 0))
                      .build())
        .build();
    const model::test_result result_10(model::test_result_failed,
                                       "Exit failure");

    const model::test_program test_program_9 = model::test_program_builder(
        "atf", fs::path("complex_test"), fs::path("/usr/tests"),
        "suite-name")
        .add_test_case("this_passes")
        .add_test_case("this_fails",
                       model::metadata_builder()
                       .set_description("Test description")
                       .set_required_user("root")
                       .build())
        .build();
    const model::test_result result_11(model::test_result_passed);
    const model::test_result result_12(model::test_result_failed,
                                       "Some reason");

    store::results_iterator iter = transaction.get_results();
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_9, *iter.test_program());
    ATF_REQUIRE_EQ("this_fails", iter.test_case_name());
    ATF_REQUIRE_EQ(result_12, iter.result());
    ATF_REQUIRE(iter.stdout_contents().empty());
    ATF_REQUIRE(iter.stderr_contents().empty());
    ATF_REQUIRE_EQ(1357644397100000LL, iter.start_time().to_microseconds());
    ATF_REQUIRE_EQ(1357644399005000LL, iter.end_time().to_microseconds());
    ATF_REQUIRE_EQ(datetime::delta(1, 905000), iter.elapsed_time());

    ++iter;
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_9, *iter.test_program());
    ATF_REQUIRE_EQ("this_passes", iter.test_case_name());
    ATF_REQUIRE_EQ(result_11, iter.result());
    ATF_REQUIRE(iter.stdout_contents().empty());
    ATF_REQUIRE(iter.stderr_contents().empty());
    ATF_REQUIRE_EQ(1357644396500000LL, iter.start_time().to_microseconds());
    ATF_REQUIRE_EQ(1357644397000000LL, iter.end_time().to_microseconds());
    ATF_REQUIRE_EQ(datetime::delta(0, 500000), iter.elapsed_time());

    ++iter;
    ATF_REQUIRE(iter);
    ATF_REQUIRE_EQ(test_program_8, *iter.test_program());
    ATF_REQUIRE_EQ("main", iter.test_case_name());
    ATF_REQUIRE_EQ(result_10, iter.result());
    ATF_REQUIRE_EQ("Test stdout", iter.stdout_contents());
    ATF_REQUIRE_EQ("Test stderr", iter.stderr_contents());
    ATF_REQUIRE_EQ(1357644395000000LL, iter.start_time().to_microseconds());
    ATF_REQUIRE_EQ(1357644396000000LL, iter.end_time().to_microseconds());
    ATF_REQUIRE_EQ(datetime::delta(1, 0), iter.elapsed_time());

    ++iter;
    ATF_REQUIRE(!iter);
}


/// \return The files required by the version 3 duration migration tests.
static std::string
elapsed_time_migration_files(void)
{
    return (store::detail::schema_file().branch_path() /
            "schema_v3.sql").str() + " " +
        testdata_file("testdata_v3_2.sql").str() + " " +
        store::detail::migration_file(3, 4).str();
}


/// Checks the duration imported from a version 3 results file.
///
/// \param start_time Calendar start timestamp in microseconds.
/// \param end_time Calendar end timestamp in microseconds.
/// \param expected Expected migrated duration.
static void
check_migrated_elapsed_time(const int64_t start_time, const int64_t end_time,
                           const datetime::delta& expected)
{
    const fs::path testpath("test.db");
    sqlite::database db = sqlite::database::open(
        testpath, sqlite::open_readwrite | sqlite::open_create);
    db.exec(utils::read_file(store::detail::schema_file().branch_path() /
                            "schema_v3.sql"));
    db.exec(utils::read_file(testdata_file("testdata_v3_2.sql")));
    db.exec(F("UPDATE test_results SET start_time = %s, end_time = %s") %
            start_time % end_time);
    db.close();

    store::migrate_schema(testpath);
    store::read_backend backend = store::read_backend::open_ro(testpath);
    store::read_transaction tx = backend.start_read();
    store::results_iterator iter = tx.get_results();
    for (std::size_t i = 0; i < 5; ++i, ++iter) {
        ATF_REQUIRE(iter);
        ATF_REQUIRE_EQ(start_time, iter.start_time().to_microseconds());
        ATF_REQUIRE_EQ(end_time, iter.end_time().to_microseconds());
        ATF_REQUIRE_EQ(expected, iter.elapsed_time());
    }
    ATF_REQUIRE(!iter);
}


}  // anonymous namespace


#define CURRENT_SCHEMA_TEST(dataset) \
    ATF_TEST_CASE(current_schema_ ##dataset); \
    ATF_TEST_CASE_HEAD(current_schema_ ##dataset) \
    { \
        logging::set_inmemory(); \
        const std::string required_files = \
            store::detail::schema_file().str() + " " + \
            testdata_file("testdata_v3_" #dataset ".sql").str() + " " + \
            testdata_file("testdata_v4.sql").str(); \
        set_md_var("require.files", required_files); \
    } \
    ATF_TEST_CASE_BODY(current_schema_ ##dataset) \
    { \
        const fs::path testpath("test.db"); \
        \
        sqlite::database db = sqlite::database::open( \
            testpath, sqlite::open_readwrite | sqlite::open_create); \
        db.exec(utils::read_file(store::detail::schema_file())); \
        db.exec(utils::read_file(testdata_file(\
            "testdata_v3_" #dataset ".sql"))); \
        db.exec(utils::read_file(testdata_file("testdata_v4.sql"))); \
        db.close(); \
        \
        check_action_ ## dataset (testpath); \
    }
CURRENT_SCHEMA_TEST(1);
CURRENT_SCHEMA_TEST(2);
CURRENT_SCHEMA_TEST(3);
CURRENT_SCHEMA_TEST(4);


#define MIGRATE_SCHEMA_TEST(from_version) \
    ATF_TEST_CASE(migrate_schema__from_v ##from_version); \
    ATF_TEST_CASE_HEAD(migrate_schema__from_v ##from_version) \
    { \
        logging::set_inmemory(); \
        \
        const char* schema = "schema_v" #from_version ".sql"; \
        const char* testdata = "testdata_v" #from_version ".sql"; \
        \
        std::string required_files = \
            testdata_file(schema).str() + " " + testdata_file(testdata).str(); \
        for (int i = from_version; i < store::detail::current_schema_version; \
             ++i) \
            required_files += " " + store::detail::migration_file( \
                i, i + 1).str(); \
        \
        set_md_var("require.files", required_files); \
    } \
    ATF_TEST_CASE_BODY(migrate_schema__from_v ##from_version) \
    { \
        const char* schema = "schema_v" #from_version ".sql"; \
        const char* testdata = "testdata_v" #from_version ".sql"; \
        \
        const fs::path testpath("test.db"); \
        \
        sqlite::database db = sqlite::database::open( \
            testpath, sqlite::open_readwrite | sqlite::open_create); \
        db.exec(utils::read_file(testdata_file(schema))); \
        db.exec(utils::read_file(testdata_file(testdata))); \
        db.close(); \
        \
        store::migrate_schema(fs::path("test.db")); \
        \
        check_action_2(fs::path(".kyua/store/" \
            "results.test_suite_root.20130108-111331-000000.db")); \
        check_action_3(fs::path(".kyua/store/" \
            "results.usr_tests.20130108-123832-000000.db")); \
        check_action_4(fs::path(".kyua/store/" \
            "results.usr_tests.20130108-112635-000000.db")); \
    }
MIGRATE_SCHEMA_TEST(1);
MIGRATE_SCHEMA_TEST(2);


ATF_TEST_CASE(migrate_schema__from_v3__positive);
ATF_TEST_CASE_HEAD(migrate_schema__from_v3__positive)
{
    logging::set_inmemory();
    set_md_var("require.files", elapsed_time_migration_files());
}
ATF_TEST_CASE_BODY(migrate_schema__from_v3__positive)
{
    check_migrated_elapsed_time(2000000, 3234567, datetime::delta(1, 234567));
}


ATF_TEST_CASE(migrate_schema__from_v3__zero);
ATF_TEST_CASE_HEAD(migrate_schema__from_v3__zero)
{
    logging::set_inmemory();
    set_md_var("require.files", elapsed_time_migration_files());
}
ATF_TEST_CASE_BODY(migrate_schema__from_v3__zero)
{
    check_migrated_elapsed_time(2000000, 2000000, datetime::delta(0, 0));
}


ATF_TEST_CASE(migrate_schema__from_v3__backward);
ATF_TEST_CASE_HEAD(migrate_schema__from_v3__backward)
{
    logging::set_inmemory();
    set_md_var("require.files", elapsed_time_migration_files());
}
ATF_TEST_CASE_BODY(migrate_schema__from_v3__backward)
{
    check_migrated_elapsed_time(2000000, 1999999, datetime::delta(0, 0));
}


ATF_TEST_CASE(migrate_schema__v4_rollback);
ATF_TEST_CASE_HEAD(migrate_schema__v4_rollback)
{
    logging::set_inmemory();
    set_md_var("require.files",
               (store::detail::schema_file().branch_path() /
                "schema_v3.sql").str() + " " +
               store::detail::migration_file(3, 4).str());
}
ATF_TEST_CASE_BODY(migrate_schema__v4_rollback)
{
    const fs::path testpath("test.db");
    sqlite::database db = sqlite::database::open(
        testpath, sqlite::open_readwrite | sqlite::open_create);
    db.exec(utils::read_file(store::detail::schema_file().branch_path() /
                            "schema_v3.sql"));
    db.exec("CREATE TRIGGER reject_upgrade BEFORE INSERT ON metadata "
            "WHEN NEW.schema_version = 4 "
            "BEGIN SELECT RAISE(ABORT, 'Rejected upgrade'); END");
    db.close();

    ATF_REQUIRE_THROW_RE(store::error, "Rejected upgrade",
                         store::migrate_schema(testpath));
    ATF_REQUIRE(fs::exists(fs::path("test.db.v3.backup")));
    db = sqlite::database::open(testpath, sqlite::open_readonly);
    ATF_REQUIRE_EQ(3, store::metadata::fetch_latest(db).schema_version());
    sqlite::statement stmt = db.create_statement(
        "PRAGMA table_info(test_results)");
    while (stmt.step())
        ATF_REQUIRE(stmt.safe_column_text("name") != "elapsed_time");
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, migrate_schema__v4_rollback);

    ATF_ADD_TEST_CASE(tcs, migrate_schema__from_v3__positive);
    ATF_ADD_TEST_CASE(tcs, migrate_schema__from_v3__zero);
    ATF_ADD_TEST_CASE(tcs, migrate_schema__from_v3__backward);

    ATF_ADD_TEST_CASE(tcs, current_schema_1);
    ATF_ADD_TEST_CASE(tcs, current_schema_2);
    ATF_ADD_TEST_CASE(tcs, current_schema_3);
    ATF_ADD_TEST_CASE(tcs, current_schema_4);

    ATF_ADD_TEST_CASE(tcs, migrate_schema__from_v1);
    ATF_ADD_TEST_CASE(tcs, migrate_schema__from_v2);
}
