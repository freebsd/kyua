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

#include "engine/report_junit.hpp"

#include <sstream>
#include <vector>

#include <atf-c++.hpp>

#include "engine/context.hpp"
#include "engine/drivers/scan_results.hpp"
#include "engine/metadata.hpp"
#include "engine/test_result.hpp"
#include "store/write_backend.hpp"
#include "store/write_transaction.hpp"
#include "utils/datetime.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/units.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace scan_results = engine::drivers::scan_results;
namespace units = utils::units;

using utils::none;


namespace {


/// Formatted metadata for a test case with defaults.
static const char* const default_metadata =
    "allowed_architectures is empty\n"
    "allowed_platforms is empty\n"
    "description is empty\n"
    "has_cleanup = false\n"
    "required_configs is empty\n"
    "required_files is empty\n"
    "required_memory = 0\n"
    "required_programs is empty\n"
    "required_user is empty\n"
    "timeout = 300\n";


/// Formatted metadata for a test case constructed with the "with_metadata" flag
/// set to true in add_tests.
static const char* const overriden_metadata =
    "allowed_architectures is empty\n"
    "allowed_platforms is empty\n"
    "description = Textual description\n"
    "has_cleanup = false\n"
    "required_configs is empty\n"
    "required_files is empty\n"
    "required_memory = 0\n"
    "required_programs is empty\n"
    "required_user is empty\n"
    "timeout = 5678\n";


/// Adds a new action to the given database.
///
/// \param tx Transaction to use for the writes to the database.
/// \param env_vars Number of environment variables to add to the context.
static void
add_action(store::write_transaction& tx, const std::size_t env_vars)
{
    std::map< std::string, std::string > env;
    for (std::size_t i = 0; i < env_vars; i++)
        env[F("VAR%s") % i] = F("Value %s") % i;
    const engine::context context(fs::path("/root"), env);
    (void)tx.put_context(context);
}


/// Adds a new test program with various test cases to the given action.
///
/// \param tx Transaction to use for the writes to the database.
/// \param prog Test program name.
/// \param results Collection of results for the added test cases.  The size of
///     this vector indicates the number of tests in the test program.
/// \param with_metadata Whether to add metadata overrides to the test cases.
/// \param with_output Whether to add stdout/stderr messages to the test cases.
static void
add_tests(store::write_transaction& tx,
          const char* prog,
          const std::vector< engine::test_result >& results,
          const bool with_metadata, const bool with_output)
{
    const engine::test_program test_program(
        "plain", fs::path(prog), fs::path("/root"), "suite",
        engine::metadata_builder().build());
    const int64_t tp_id = tx.put_test_program(test_program);

    for (std::size_t j = 0; j < results.size(); j++) {
        engine::metadata_builder builder;
        if (with_metadata) {
            builder.set_description("Textual description");
            builder.set_timeout(datetime::delta(5678, 0));
        }
        const engine::test_case test_case("plain", test_program, F("t%s") % j,
                                          builder.build());
        const int64_t tc_id = tx.put_test_case(test_case, tp_id);
        const datetime::timestamp start =
            datetime::timestamp::from_microseconds(0);
        const datetime::timestamp end =
            datetime::timestamp::from_microseconds(j * 1000000 + 500000);
        tx.put_result(results[j], tc_id, start, end);

        if (with_output) {
            atf::utils::create_file("fake-out", F("stdout file %s") % j);
            tx.put_test_case_file("__STDOUT__", fs::path("fake-out"), tc_id);
            atf::utils::create_file("fake-err", F("stderr file %s") % j);
            tx.put_test_case_file("__STDERR__", fs::path("fake-err"), tc_id);
        }
    }
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(junit_classname);
ATF_TEST_CASE_BODY(junit_classname)
{
    const engine::test_program test_program(
        "plain", fs::path("dir1/dir2/program"), fs::path("/root"), "suite",
        engine::metadata_builder().build());

    ATF_REQUIRE_EQ("dir1.dir2.program", engine::junit_classname(test_program));
}


ATF_TEST_CASE_WITHOUT_HEAD(junit_duration);
ATF_TEST_CASE_BODY(junit_duration)
{
    ATF_REQUIRE_EQ("0.457", engine::junit_duration(datetime::delta(0, 456700)));
    ATF_REQUIRE_EQ("3.120", engine::junit_duration(datetime::delta(3, 120000)));
    ATF_REQUIRE_EQ("5.000", engine::junit_duration(datetime::delta(5, 0)));
}


ATF_TEST_CASE_WITHOUT_HEAD(junit_metadata__defaults);
ATF_TEST_CASE_BODY(junit_metadata__defaults)
{
    const engine::metadata metadata = engine::metadata_builder().build();

    const std::string expected = std::string()
        + engine::junit_metadata_prefix
        + default_metadata
        + engine::junit_metadata_suffix;

    ATF_REQUIRE_EQ(expected, engine::junit_metadata(metadata));
}


ATF_TEST_CASE_WITHOUT_HEAD(junit_metadata__overrides);
ATF_TEST_CASE_BODY(junit_metadata__overrides)
{
    const engine::metadata metadata = engine::metadata_builder()
        .add_allowed_architecture("arch1")
        .add_allowed_platform("platform1")
        .set_description("This is a test")
        .set_has_cleanup(true)
        .add_required_config("config1")
        .add_required_file(fs::path("file1"))
        .set_required_memory(units::bytes(123))
        .add_required_program(fs::path("prog1"))
        .set_required_user("root")
        .set_timeout(datetime::delta(10, 0))
        .build();

    const std::string expected = std::string()
        + engine::junit_metadata_prefix
        + "allowed_architectures = arch1\n"
        + "allowed_platforms = platform1\n"
        + "description = This is a test\n"
        + "has_cleanup = true\n"
        + "required_configs = config1\n"
        + "required_files = file1\n"
        + "required_memory = 123\n"
        + "required_programs = prog1\n"
        + "required_user = root\n"
        + "timeout = 10\n"
        + engine::junit_metadata_suffix;

    ATF_REQUIRE_EQ(expected, engine::junit_metadata(metadata));
}


ATF_TEST_CASE_WITHOUT_HEAD(report_junit_hooks__minimal);
ATF_TEST_CASE_BODY(report_junit_hooks__minimal)
{
    store::write_backend backend = store::write_backend::open_rw(
        fs::path("test.db"));
    store::write_transaction tx = backend.start_write();
    add_action(tx, 0);
    tx.commit();
    backend.close();

    std::ostringstream output;

    engine::report_junit_hooks hooks(output);
    scan_results::drive(fs::path("test.db"), hooks);

    const char* expected =
        "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n"
        "<testsuite>\n"
        "<properties>\n"
        "<property name=\"cwd\" value=\"/root\"/>\n"
        "</properties>\n"
        "</testsuite>\n";
    ATF_REQUIRE_EQ(expected, output.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(report_junit_hooks__some_tests);
ATF_TEST_CASE_BODY(report_junit_hooks__some_tests)
{
    using engine::test_result;

    std::vector< test_result > results1;
    results1.push_back(test_result(test_result::broken, "Broken"));
    results1.push_back(test_result(test_result::expected_failure, "XFail"));
    results1.push_back(test_result(test_result::failed, "Failed"));
    std::vector< test_result > results2;
    results2.push_back(test_result(test_result::passed));
    results2.push_back(test_result(test_result::skipped, "Skipped"));

    store::write_backend backend = store::write_backend::open_rw(
        fs::path("test.db"));
    store::write_transaction tx = backend.start_write();
    add_action(tx, 2);
    add_tests(tx, "dir/prog-1", results1, false, false);
    add_tests(tx, "dir/sub/prog-2", results2, true, true);
    tx.commit();
    backend.close();

    std::ostringstream output;

    engine::report_junit_hooks hooks(output);
    scan_results::drive(fs::path("test.db"), hooks);

    const std::string expected = std::string() +
        "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n"
        "<testsuite>\n"
        "<properties>\n"
        "<property name=\"cwd\" value=\"/root\"/>\n"
        "<property name=\"env.VAR0\" value=\"Value 0\"/>\n"
        "<property name=\"env.VAR1\" value=\"Value 1\"/>\n"
        "</properties>\n"

        "<testcase classname=\"dir.prog-1\" name=\"t0\" time=\"0.500\">\n"
        "<error message=\"Broken\"/>\n"
        "<system-err>"
        + engine::junit_metadata_prefix +
        default_metadata
        + engine::junit_metadata_suffix +
        "&lt;EMPTY&gt;\n"
        "</system-err>\n"
        "</testcase>\n"

        "<testcase classname=\"dir.prog-1\" name=\"t1\" time=\"1.500\">\n"
        "<system-err>"
        "Expected failure result details\n"
        "-------------------------------\n"
        "\n"
        "XFail\n"
        "\n"
        + engine::junit_metadata_prefix +
        default_metadata
        + engine::junit_metadata_suffix +
        "&lt;EMPTY&gt;\n"
        "</system-err>\n"
        "</testcase>\n"

        "<testcase classname=\"dir.prog-1\" name=\"t2\" time=\"2.500\">\n"
        "<failure message=\"Failed\"/>\n"
        "<system-err>"
        + engine::junit_metadata_prefix +
        default_metadata
        +  engine::junit_metadata_suffix +
        "&lt;EMPTY&gt;\n"
        "</system-err>\n"
        "</testcase>\n"

        "<testcase classname=\"dir.sub.prog-2\" name=\"t0\" time=\"0.500\">\n"
        "<system-out>stdout file 0</system-out>\n"
        "<system-err>"
        + engine::junit_metadata_prefix +
        overriden_metadata
        + engine::junit_metadata_suffix +
        "stderr file 0</system-err>\n"
        "</testcase>\n"

        "<testcase classname=\"dir.sub.prog-2\" name=\"t1\" time=\"1.500\">\n"
        "<skipped/>\n"
        "<system-out>stdout file 1</system-out>\n"
        "<system-err>"
        "Skipped result details\n"
        "----------------------\n"
        "\n"
        "Skipped\n"
        "\n"
        + engine::junit_metadata_prefix +
        overriden_metadata
        + engine::junit_metadata_suffix +
        "stderr file 1</system-err>\n"
        "</testcase>\n"

        "</testsuite>\n";
    ATF_REQUIRE_EQ(expected, output.str());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, junit_classname);

    ATF_ADD_TEST_CASE(tcs, junit_duration);

    ATF_ADD_TEST_CASE(tcs, junit_metadata__defaults);
    ATF_ADD_TEST_CASE(tcs, junit_metadata__overrides);

    ATF_ADD_TEST_CASE(tcs, report_junit_hooks__minimal);
    ATF_ADD_TEST_CASE(tcs, report_junit_hooks__some_tests);
}
