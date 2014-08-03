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

#include "text.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <atf-c.h>

#include "error.h"


static kyua_error_t call_vprintf(char** output, const char* format, ...)
    KYUA_DEFS_FORMAT_PRINTF(2, 3);


/// Invokes kyua_text_vprintf() based on a set of variable arguments.
///
/// \param [out] output Output pointer of kyua_text_vprintf().
/// \param format Formatting string to use.
/// \param ... Variable arguments to pack in a va_list to use.
///
/// \return The return value of the wrapped function.
static kyua_error_t
call_vprintf(char** output, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    kyua_error_t error = kyua_text_vprintf(output, format, ap);
    va_end(ap);
    return error;
}


ATF_TC_WITHOUT_HEAD(fgets_no_newline__ok);
ATF_TC_BODY(fgets_no_newline__ok, tc)
{
    atf_utils_create_file("test.txt", "Line one\nSecond line\nLine 3");

    char buffer[20];
    FILE* input = fopen("test.txt", "r");

    ATF_REQUIRE(kyua_text_fgets_no_newline(buffer, sizeof(buffer), input)
                == buffer);
    ATF_REQUIRE_STREQ("Line one", buffer);

    ATF_REQUIRE(kyua_text_fgets_no_newline(buffer, sizeof(buffer), input)
                == buffer);
    ATF_REQUIRE_STREQ("Second line", buffer);

    ATF_REQUIRE(kyua_text_fgets_no_newline(buffer, sizeof(buffer), input)
                == buffer);
    ATF_REQUIRE_STREQ("Line 3", buffer);

    ATF_REQUIRE(kyua_text_fgets_no_newline(buffer, sizeof(buffer), input)
                == NULL);

    fclose(input);
}


ATF_TC_WITHOUT_HEAD(fgets_no_newline__overflow);
ATF_TC_BODY(fgets_no_newline__overflow, tc)
{
    atf_utils_create_file("test.txt", "0123456789\nabcdef\n");

    char buffer[8];
    FILE* input = fopen("test.txt", "r");

    ATF_REQUIRE(kyua_text_fgets_no_newline(buffer, sizeof(buffer), input)
                == buffer);
    ATF_REQUIRE_STREQ("0123456", buffer);

    ATF_REQUIRE(kyua_text_fgets_no_newline(buffer, sizeof(buffer), input)
                == buffer);
    ATF_REQUIRE_STREQ("789", buffer);

    ATF_REQUIRE(kyua_text_fgets_no_newline(buffer, sizeof(buffer), input)
                == buffer);
    ATF_REQUIRE_STREQ("abcdef", buffer);

    ATF_REQUIRE(kyua_text_fgets_no_newline(buffer, sizeof(buffer), input)
                == NULL);

    fclose(input);
}


ATF_TC_WITHOUT_HEAD(fgets_error__unexpected_eof);
ATF_TC_BODY(fgets_error__unexpected_eof, tc)
{
    atf_utils_create_file("test.txt", "Some line\n");

    char buffer[1024];
    FILE* input = fopen("test.txt", "r");

    ATF_REQUIRE(kyua_text_fgets_no_newline(buffer, sizeof(buffer), input)
                == buffer);
    ATF_REQUIRE_STREQ("Some line", buffer);

    ATF_REQUIRE(kyua_text_fgets_no_newline(buffer, sizeof(buffer), input)
                == NULL);
    kyua_error_t error = kyua_text_fgets_error(input, "Foo bar");
    ATF_REQUIRE(kyua_error_is_set(error));
    kyua_error_format(error, buffer, sizeof(buffer));
    ATF_REQUIRE_STREQ("Foo bar: unexpected EOF", buffer);
    kyua_error_free(error);

    fclose(input);
}


ATF_TC_WITHOUT_HEAD(fgets_error__libc_error);
ATF_TC_BODY(fgets_error__libc_error, tc)
{
    atf_utils_create_file("test.txt", "Some line\n");

    char buffer[1024];
    FILE* input = fopen("test.txt", "w");

    ATF_REQUIRE(kyua_text_fgets_no_newline(buffer, sizeof(buffer), input)
                == NULL);
    kyua_error_t error = kyua_text_fgets_error(input, "Foo bar");
    ATF_REQUIRE(kyua_error_is_set(error));
    kyua_error_format(error, buffer, sizeof(buffer));
    ATF_REQUIRE_MATCH("^Foo bar: .*", buffer);
    kyua_error_free(error);

    fclose(input);
}


ATF_TC_WITHOUT_HEAD(find_first_of__found);
ATF_TC_BODY(find_first_of__found, tc)
{
    char text[] = "abcdedcba";
    char* ptr = kyua_text_find_first_of(text, "ce");
    ATF_REQUIRE_EQ(text + 2, ptr);
    ptr = kyua_text_find_first_of(ptr + 1, "ce");
    ATF_REQUIRE_EQ(text + 4, ptr);
    ptr = kyua_text_find_first_of(ptr + 1, "ce");
    ATF_REQUIRE_EQ(text + 6, ptr);
    ptr = kyua_text_find_first_of(ptr + 1, "ce");
    ATF_REQUIRE_EQ(NULL, ptr);
}


ATF_TC_WITHOUT_HEAD(find_first_of__not_found);
ATF_TC_BODY(find_first_of__not_found, tc)
{
    char text[] = "abcdedcba";
    char* ptr = kyua_text_find_first_of(text, "g6");
    ATF_REQUIRE_EQ(NULL, ptr);
}


ATF_TC_WITHOUT_HEAD(printf__empty);
ATF_TC_BODY(printf__empty, tc)
{
    char* buffer;
    kyua_error_t error = kyua_text_printf(&buffer, "%s", "");
    ATF_REQUIRE(!kyua_error_is_set(error));
    ATF_REQUIRE_STREQ("", buffer);
}


ATF_TC_WITHOUT_HEAD(printf__some);
ATF_TC_BODY(printf__some, tc)
{
    char* buffer;
    kyua_error_t error = kyua_text_printf(&buffer, "this is %d %s", 123, "foo");
    ATF_REQUIRE(!kyua_error_is_set(error));
    ATF_REQUIRE_STREQ("this is 123 foo", buffer);
    free(buffer);
}


ATF_TC_WITHOUT_HEAD(vprintf__empty);
ATF_TC_BODY(vprintf__empty, tc)
{
    char* buffer;
    kyua_error_t error = call_vprintf(&buffer, "%s", "");
    ATF_REQUIRE(!kyua_error_is_set(error));
    ATF_REQUIRE_STREQ("", buffer);
}


ATF_TC_WITHOUT_HEAD(vprintf__some);
ATF_TC_BODY(vprintf__some, tc)
{
    char* buffer;
    kyua_error_t error = call_vprintf(&buffer, "this is %d %s", 123, "foo");
    ATF_REQUIRE(!kyua_error_is_set(error));
    ATF_REQUIRE_STREQ("this is 123 foo", buffer);
    free(buffer);
}


ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, fgets_no_newline__ok);
    ATF_TP_ADD_TC(tp, fgets_no_newline__overflow);

    ATF_TP_ADD_TC(tp, fgets_error__unexpected_eof);
    ATF_TP_ADD_TC(tp, fgets_error__libc_error);

    ATF_TP_ADD_TC(tp, find_first_of__found);
    ATF_TP_ADD_TC(tp, find_first_of__not_found);

    ATF_TP_ADD_TC(tp, printf__empty);
    ATF_TP_ADD_TC(tp, printf__some);

    ATF_TP_ADD_TC(tp, vprintf__empty);
    ATF_TP_ADD_TC(tp, vprintf__some);

    return atf_no_error();
}
