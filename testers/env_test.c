// Copyright 2012 Google Inc.
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

#include "testers/env.h"

#include <stdlib.h>
#include <string.h>

#include <atf-c.h>

#include "testers/error.h"


ATF_TC_WITHOUT_HEAD(set);
ATF_TC_BODY(set, tc)
{
    ATF_REQUIRE(strcmp(getenv("PATH"), "new value") != 0);
    kyua_env_set("PATH", "new value");
    ATF_REQUIRE(strcmp(getenv("PATH"), "new value") == 0);
}


ATF_TC_WITHOUT_HEAD(unset);
ATF_TC_BODY(unset, tc)
{
    ATF_REQUIRE(getenv("PATH") != NULL);
    kyua_env_unset("PATH");
    ATF_REQUIRE(getenv("PATH") == NULL);
}


ATF_TC_WITHOUT_HEAD(check_configuration__ok__empty);
ATF_TC_BODY(check_configuration__ok__empty, tc)
{
    const char* const config[] = { NULL };
    const kyua_error_t error = kyua_env_check_configuration(config);
    ATF_REQUIRE(!kyua_error_is_set(error));
}


ATF_TC_WITHOUT_HEAD(check_configuration__ok__some);
ATF_TC_BODY(check_configuration__ok__some, tc)
{
    const char* const config[] = { "first=second", "bar=baz", NULL };
    const kyua_error_t error = kyua_env_check_configuration(config);
    ATF_REQUIRE(!kyua_error_is_set(error));
}


/// Executes a single check_configuration__fail test.
///
/// \param var_value The invalid var_value pair to validate.
/// \param exp_error Regexp to use to validate the error message.
static void
do_check_configuration_fail(const char* var_value, const char* exp_error)
{
    const char* const config[] = { "first=second", var_value, "bar=baz", NULL };
    const kyua_error_t error = kyua_env_check_configuration(config);
    ATF_REQUIRE(kyua_error_is_set(error));

    char buffer[1024];
    kyua_error_format(error, buffer, sizeof(buffer));
    ATF_REQUIRE_MATCH(exp_error, buffer);

    kyua_error_free(error);
}


ATF_TC_WITHOUT_HEAD(check_configuration__fail);
ATF_TC_BODY(check_configuration__fail, tc)
{
    do_check_configuration_fail("no-equal", "Invalid variable 'no-equal'");
    do_check_configuration_fail("", "Invalid variable ''");
    do_check_configuration_fail("=foo", "Invalid variable '=foo'");
}


ATF_TC_WITHOUT_HEAD(set_configuration__empty);
ATF_TC_BODY(set_configuration__empty, tc)
{
    const char* const config[] = { NULL };
    const kyua_error_t error = kyua_env_set_configuration(config);
    ATF_REQUIRE(!kyua_error_is_set(error));
}


ATF_TC_WITHOUT_HEAD(set_configuration__some);
ATF_TC_BODY(set_configuration__some, tc)
{
    const char* const config[] = { "first=second", "bar=baz", NULL };
    const kyua_error_t error = kyua_env_set_configuration(config);
    ATF_REQUIRE(!kyua_error_is_set(error));

    const char* first_value = getenv("TEST_ENV_first");
    ATF_REQUIRE(first_value != NULL);
    ATF_REQUIRE_STREQ("second", first_value);

    const char* bar_value = getenv("TEST_ENV_bar");
    ATF_REQUIRE(bar_value != NULL);
    ATF_REQUIRE_STREQ("baz", bar_value);

    ATF_REQUIRE(getenv("first") == NULL);
    ATF_REQUIRE(getenv("second") == NULL);
    ATF_REQUIRE(getenv("bar") == NULL);
    ATF_REQUIRE(getenv("baz") == NULL);
}


ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, set);
    ATF_TP_ADD_TC(tp, unset);

    ATF_TP_ADD_TC(tp, check_configuration__ok__empty);
    ATF_TP_ADD_TC(tp, check_configuration__ok__some);
    ATF_TP_ADD_TC(tp, check_configuration__fail);

    ATF_TP_ADD_TC(tp, set_configuration__empty);
    ATF_TP_ADD_TC(tp, set_configuration__some);

    return atf_no_error();
}
