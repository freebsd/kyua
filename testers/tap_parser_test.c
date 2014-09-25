// Copyright 2013 Google Inc.
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

#include "testers/tap_parser.h"

#include <atf-c.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "testers/error.h"


/// Compares two kyua_tap_summary_t objects and fails the test if they differ.
///
/// \param expected The expected value for the TAP summary.
/// \param actual The expected value of the TAP summary.
static void
summary_require_eq(const kyua_tap_summary_t* expected,
                   const kyua_tap_summary_t* actual)
{
    if (expected->parse_error != actual->parse_error)
        ATF_REQUIRE_STREQ(expected->parse_error, actual->parse_error);
    ATF_REQUIRE_EQ(expected->bail_out, actual->bail_out);
    ATF_REQUIRE_EQ(expected->first_index, actual->first_index);
    ATF_REQUIRE_EQ(expected->last_index, actual->last_index);
    if (expected->all_skipped_reason != actual->all_skipped_reason)
        ATF_REQUIRE_STREQ(expected->all_skipped_reason,
                          actual->all_skipped_reason);
    ATF_REQUIRE_EQ(expected->ok_count, actual->ok_count);
    ATF_REQUIRE_EQ(expected->not_ok_count, actual->not_ok_count);
}


ATF_TC_WITHOUT_HEAD(try_parse_plan__ok);
ATF_TC_BODY(try_parse_plan__ok, tc)
{
    kyua_tap_summary_t summary;
    kyua_tap_summary_init(&summary);
    ATF_REQUIRE(!kyua_error_is_set(kyua_tap_try_parse_plan("3..85", &summary)));
    ATF_REQUIRE_EQ(NULL, summary.parse_error);
    ATF_REQUIRE_EQ(3, summary.first_index);
    ATF_REQUIRE_EQ(85, summary.last_index);
    ATF_REQUIRE_EQ(NULL, summary.all_skipped_reason);
    kyua_tap_summary_fini(&summary);
}


ATF_TC_WITHOUT_HEAD(try_parse_plan__ok__skip_without_reason);
ATF_TC_BODY(try_parse_plan__ok__skip_without_reason, tc)
{
    kyua_tap_summary_t summary;
    kyua_tap_summary_init(&summary);
    ATF_REQUIRE(!kyua_error_is_set(kyua_tap_try_parse_plan(
        "1..0 unrecognized # garbage skip", &summary)));
    ATF_REQUIRE_EQ(NULL, summary.parse_error);
    ATF_REQUIRE_EQ(1, summary.first_index);
    ATF_REQUIRE_EQ(0, summary.last_index);
    ATF_REQUIRE_STREQ("No reason specified", summary.all_skipped_reason);
    kyua_tap_summary_fini(&summary);
}


ATF_TC_WITHOUT_HEAD(try_parse_plan__ok__skip_with_reason);
ATF_TC_BODY(try_parse_plan__ok__skip_with_reason, tc)
{
    kyua_tap_summary_t summary;
    kyua_tap_summary_init(&summary);
    ATF_REQUIRE(!kyua_error_is_set(kyua_tap_try_parse_plan(
        "1..0 # SKIP all the things", &summary)));
    ATF_REQUIRE_EQ(NULL, summary.parse_error);
    ATF_REQUIRE_EQ(1, summary.first_index);
    ATF_REQUIRE_EQ(0, summary.last_index);
    ATF_REQUIRE_STREQ("all the things", summary.all_skipped_reason);
    kyua_tap_summary_fini(&summary);
}


ATF_TC_WITHOUT_HEAD(try_parse_plan__invalid__skip);
ATF_TC_BODY(try_parse_plan__invalid__skip, tc)
{
    kyua_tap_summary_t summary;
    kyua_tap_summary_init(&summary);
    ATF_REQUIRE(!kyua_error_is_set(kyua_tap_try_parse_plan(
        "1..3 # skip", &summary)));
    ATF_REQUIRE_MATCH("Skipped test plan has invalid range",
                      summary.parse_error);
    kyua_tap_summary_fini(&summary);
}


ATF_TC_WITHOUT_HEAD(try_parse_plan__reversed);
ATF_TC_BODY(try_parse_plan__reversed, tc)
{
    kyua_tap_summary_t summary;
    kyua_tap_summary_init(&summary);
    ATF_REQUIRE(!kyua_error_is_set(kyua_tap_try_parse_plan("8..5", &summary)));
    ATF_REQUIRE_MATCH("is reversed", summary.parse_error);
    kyua_tap_summary_fini(&summary);
}


ATF_TC_WITHOUT_HEAD(try_parse_plan__insane);
ATF_TC_BODY(try_parse_plan__insane, tc)
{
    kyua_tap_summary_t summary;
    kyua_tap_summary_init(&summary);
    ATF_REQUIRE(!kyua_error_is_set(kyua_tap_try_parse_plan(
        "120830981209831..234891793874080981092803981092312", &summary)));
    ATF_REQUIRE_MATCH("too long", summary.parse_error);
    kyua_tap_summary_fini(&summary);
}


/// Executes kyua_tap_parse expecting success and validates the results.
///
/// \param contents The text to parse.
/// \param expected_summary Expected results of the parsing.
static void
ok_test(const char* contents, const kyua_tap_summary_t* expected_summary)
{
    atf_utils_create_file("input.txt", "%s", contents);

    const int fd = open("input.txt", O_RDONLY);
    ATF_REQUIRE(fd != -1);

    FILE* output = fopen("output.txt", "w");
    ATF_REQUIRE(output != NULL);

    kyua_tap_summary_t summary;
    ATF_REQUIRE(!kyua_error_is_set(kyua_tap_parse(fd, output, &summary)));
    fclose(output);

    summary_require_eq(expected_summary, &summary);
    ATF_REQUIRE(atf_utils_compare_file("output.txt", contents));
    kyua_tap_summary_fini(&summary);
}


ATF_TC_WITHOUT_HEAD(parse__ok__pass);
ATF_TC_BODY(parse__ok__pass, tc)
{
    const char* contents =
        "1..8\n"
        "ok - 1\n"
        "    Some diagnostic message\n"
        "ok - 2 This test also passed\n"
        "garbage line\n"
        "ok - 3 This test passed\n"
        "not ok 4 # SKIP Some reason\n"
        "not ok 5 # TODO Another reason\n"
        "ok - 6 Doesn't make a difference SKIP\n"
        "ok - 7 Doesn't make a difference either TODO\n"
        "ok # Also works without a number\n";

    kyua_tap_summary_t summary;
    kyua_tap_summary_init(&summary);
    summary.first_index = 1;
    summary.last_index = 8;
    summary.ok_count = 8;
    summary.not_ok_count = 0;

    ok_test(contents, &summary);
    kyua_tap_summary_fini(&summary);
}


ATF_TC_WITHOUT_HEAD(parse__ok__fail);
ATF_TC_BODY(parse__ok__fail, tc)
{
    const char* contents =
        "garbage line\n"
        "not ok - 1 This test failed\n"
        "ok - 2 This test passed\n"
        "not ok - 3 This test failed\n"
        "1..6\n"
        "not ok - 4 This test failed\n"
        "ok - 5 This test passed\n"
        "not ok # Fails as well without a number\n";

    kyua_tap_summary_t summary;
    kyua_tap_summary_init(&summary);
    summary.first_index = 1;
    summary.last_index = 6;
    summary.ok_count = 2;
    summary.not_ok_count = 4;

    ok_test(contents, &summary);
    kyua_tap_summary_fini(&summary);
}


ATF_TC_WITHOUT_HEAD(parse__ok__skip);
ATF_TC_BODY(parse__ok__skip, tc)
{
    const char* contents =
        "1..0 skip Some reason for skipping\n"
        "ok - 1\n"
        "    Some diagnostic message\n"
        "ok - 6 Doesn't make a difference SKIP\n"
        "ok - 7 Doesn't make a difference either TODO\n";

    kyua_tap_summary_t summary;
    kyua_tap_summary_init(&summary);
    summary.first_index = 1;
    summary.last_index = 0;
    summary.all_skipped_reason = strdup("Some reason for skipping");
    // Don't matter for our validation, but we need to set these due to the
    // simplicity of our validation in ok_test.
    summary.ok_count = 3;
    summary.not_ok_count = 0;

    ok_test(contents, &summary);
    kyua_tap_summary_fini(&summary);
}


ATF_TC_WITHOUT_HEAD(parse__ok__plan_at_the_end);
ATF_TC_BODY(parse__ok__plan_at_the_end, tc)
{
    const char* contents =
        "ok - 1\n"
        "    Some diagnostic message\n"
        "ok - 2 This test also passed\n"
        "garbage line\n"
        "ok - 3 This test passed\n"
        "not ok 4 # SKIP Some reason\n"
        "not ok 5 # TODO Another reason\n"
        "ok - 6 Doesn't make a difference SKIP\n"
        "ok - 7 Doesn't make a difference either TODO\n"
        "1..7\n";

    kyua_tap_summary_t summary;
    kyua_tap_summary_init(&summary);
    summary.first_index = 1;
    summary.last_index = 7;
    summary.ok_count = 7;
    summary.not_ok_count = 0;

    ok_test(contents, &summary);
    kyua_tap_summary_fini(&summary);
}


ATF_TC_WITHOUT_HEAD(parse__ok__stray_oks);
ATF_TC_BODY(parse__ok__stray_oks, tc)
{
    const char* contents =
        "1..3\n"
        "ok - 1\n"
        "ok\n"
        "ok - 2 This test also passed\n"
        "not ok\n"
        "ok - 3 This test passed\n";

    kyua_tap_summary_t summary;
    kyua_tap_summary_init(&summary);
    summary.first_index = 1;
    summary.last_index = 3;
    summary.ok_count = 3;
    summary.not_ok_count = 0;

    ok_test(contents, &summary);
    kyua_tap_summary_fini(&summary);
}


/// Executes kyua_tap_parse expecting a failure and validates the results.
///
/// \param contents The text to parse.
/// \param exp_output Expected output of the function.  Should be a subset of
///     contents.
/// \param exp_regex Regular expression to validate the error message.
static void
fail_test(const char* contents, const char* exp_output, const char* exp_regex)
{
    atf_utils_create_file("input.txt", "%s", contents);

    const int fd = open("input.txt", O_RDONLY);
    ATF_REQUIRE(fd != -1);

    FILE* output = fopen("output.txt", "w");
    ATF_REQUIRE(output != NULL);

    kyua_tap_summary_t summary;
    ATF_REQUIRE(!kyua_error_is_set(kyua_tap_parse(fd, output, &summary)));
    fclose(output);

    ATF_REQUIRE_MATCH(exp_regex, summary.parse_error);

    ATF_REQUIRE(atf_utils_compare_file("output.txt", exp_output));
    kyua_tap_summary_fini(&summary);
}


ATF_TC_WITHOUT_HEAD(parse__fail__double_plan);
ATF_TC_BODY(parse__fail__double_plan, tc)
{
    const char* contents =
        "garbage line\n"
        "1..5\n"
        "not ok - 1 This test failed\n"
        "ok - 2 This test passed\n"
        "1..8\n"
        "ok\n";

    const char* output =
        "garbage line\n"
        "1..5\n"
        "not ok - 1 This test failed\n"
        "ok - 2 This test passed\n"
        "1..8\n";

    fail_test(contents, output, "Output includes two test plans");
}


ATF_TC_WITHOUT_HEAD(parse__fail__inconsistent_plan);
ATF_TC_BODY(parse__fail__inconsistent_plan, tc)
{
    const char* contents =
        "1..3\n"
        "not ok - 1 This test failed\n"
        "ok - 2 This test passed\n";

    fail_test(contents, contents, "plan differs from actual executed tests");
}


ATF_TC_WITHOUT_HEAD(parse__fail__inconsistent_plan_at_the_end);
ATF_TC_BODY(parse__fail__inconsistent_plan_at_the_end, tc)
{
    const char* contents =
        "not ok - 1 This test failed\n"
        "ok - 2 This test passed\n"
        "1..3\n";

    fail_test(contents, contents, "plan differs from actual executed tests");
}


ATF_TC_WITHOUT_HEAD(parse__bail_out);
ATF_TC_BODY(parse__bail_out, tc)
{
    const char* contents =
        "1..3\n"
        "not ok - 1 This test failed\n"
        "Bail out! There is some unknown problem\n"
        "ok - 2 This test passed\n";
    atf_utils_create_file("input.txt", "%s", contents);

    const int fd = open("input.txt", O_RDONLY);
    ATF_REQUIRE(fd != -1);

    FILE* output = fopen("output.txt", "w");
    ATF_REQUIRE(output != NULL);

    kyua_tap_summary_t summary;
    ATF_REQUIRE(!kyua_error_is_set(kyua_tap_parse(fd, output, &summary)));
    fclose(output);

    ATF_REQUIRE_EQ(NULL, summary.parse_error);
    ATF_REQUIRE(summary.bail_out);

    const char* exp_output =
        "1..3\n"
        "not ok - 1 This test failed\n"
        "Bail out! There is some unknown problem\n";
    ATF_REQUIRE(atf_utils_compare_file("output.txt", exp_output));
    kyua_tap_summary_fini(&summary);
}


ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, try_parse_plan__ok);
    ATF_TP_ADD_TC(tp, try_parse_plan__ok__skip_with_reason);
    ATF_TP_ADD_TC(tp, try_parse_plan__ok__skip_without_reason);
    ATF_TP_ADD_TC(tp, try_parse_plan__invalid__skip);
    ATF_TP_ADD_TC(tp, try_parse_plan__reversed);
    ATF_TP_ADD_TC(tp, try_parse_plan__insane);

    ATF_TP_ADD_TC(tp, parse__ok__pass);
    ATF_TP_ADD_TC(tp, parse__ok__fail);
    ATF_TP_ADD_TC(tp, parse__ok__skip);
    ATF_TP_ADD_TC(tp, parse__ok__plan_at_the_end);
    ATF_TP_ADD_TC(tp, parse__ok__stray_oks);
    ATF_TP_ADD_TC(tp, parse__fail__double_plan);
    ATF_TP_ADD_TC(tp, parse__fail__inconsistent_plan);
    ATF_TP_ADD_TC(tp, parse__fail__inconsistent_plan_at_the_end);
    ATF_TP_ADD_TC(tp, parse__bail_out);

    return atf_no_error();
}
