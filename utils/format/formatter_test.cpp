// Copyright 2010, 2011 Google Inc.
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

#include <atf-c++.hpp>

#include "utils/format/exceptions.hpp"
#include "utils/format/formatter.hpp"
#include "utils/format/macros.hpp"

using utils::format::bad_format_error;
using utils::format::error;
using utils::format::extra_args_error;


ATF_TEST_CASE_WITHOUT_HEAD(no_fields);
ATF_TEST_CASE_BODY(no_fields)
{
    ATF_REQUIRE_EQ("Plain string", F("Plain string").str());
}


ATF_TEST_CASE_WITHOUT_HEAD(one_field);
ATF_TEST_CASE_BODY(one_field)
{
    ATF_REQUIRE_EQ("foo", (F("%sfoo") % "").str());
    ATF_REQUIRE_EQ(" foo", (F("%sfoo") % " ").str());
    ATF_REQUIRE_EQ("foo ", (F("foo %s") % "").str());
    ATF_REQUIRE_EQ("foo bar", (F("foo %s") % "bar").str());
    ATF_REQUIRE_EQ("foo bar baz", (F("foo %s baz") % "bar").str());
    ATF_REQUIRE_EQ("foo %s %d", (F("foo %s %s") % "%s" % "%d").str());
}


ATF_TEST_CASE_WITHOUT_HEAD(many_fields);
ATF_TEST_CASE_BODY(many_fields)
{
    ATF_REQUIRE_EQ("", (F("%s%s") % "" % "").str());
    ATF_REQUIRE_EQ("foo", (F("%s%s%s") % "" % "foo" % "").str());
    ATF_REQUIRE_EQ("some 5 text", (F("%s %d %s") % "some" % 5 % "text").str());
    ATF_REQUIRE_EQ("f%s 5 text", (F("%s %d %s") % "f%s" % 5 % "text").str());
}


ATF_TEST_CASE_WITHOUT_HEAD(escape);
ATF_TEST_CASE_BODY(escape)
{
    ATF_REQUIRE_EQ("%", F("%%").str());
    ATF_REQUIRE_EQ("foo %", F("foo %%").str());
    ATF_REQUIRE_EQ("foo bar %", (F("foo %s %%") % "bar").str());
    ATF_REQUIRE_EQ("foo % bar", (F("foo %% %s") % "bar").str());
}


ATF_TEST_CASE_WITHOUT_HEAD(extra_args_error);
ATF_TEST_CASE_BODY(extra_args_error)
{
    ATF_REQUIRE_THROW(extra_args_error, F("foo") % "bar");
    ATF_REQUIRE_THROW(extra_args_error, F("foo %%") % "bar");
    ATF_REQUIRE_THROW(extra_args_error, F("foo %s") % "bar" % "baz");
    ATF_REQUIRE_THROW(extra_args_error, F("foo %s") % "%s" % "bar");
    ATF_REQUIRE_THROW(extra_args_error, F("%s foo %s") % "bar" % "baz" % "foo");

    try {
        F("foo %s %s") % "bar" % "baz" % "something extra";
        fail("extra_args_error not raised");
    } catch (const extra_args_error& e) {
        ATF_REQUIRE_EQ("foo %s %s", e.format());
        ATF_REQUIRE_EQ("something extra", e.arg());
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(valid_formatters);
ATF_TEST_CASE_BODY(valid_formatters)
{
    ATF_REQUIRE_EQ("a", (F("%c") % 'a').str());
    ATF_REQUIRE_EQ("34", (F("%d") % 34).str());
    ATF_REQUIRE_EQ("3.5", (F("%f") % 3.5).str());
    ATF_REQUIRE_EQ("Some text", (F("%s") % "Some text").str());
    ATF_REQUIRE_EQ("-45", (F("%u") % -45).str());
}


ATF_TEST_CASE_WITHOUT_HEAD(bad_format_error);
ATF_TEST_CASE_BODY(bad_format_error)
{
    ATF_REQUIRE_THROW(bad_format_error, F("%"));
    ATF_REQUIRE_THROW(bad_format_error, F("f%"));
    ATF_REQUIRE_THROW(bad_format_error, F("foo %s baz%") % "bar");

    try {
        F("foo %s%") % "bar";
        fail("bad_format_error not raised");
    } catch (const bad_format_error& e) {
        ATF_REQUIRE_EQ("foo %s%", e.format());
    }
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, no_fields);
    ATF_ADD_TEST_CASE(tcs, one_field);
    ATF_ADD_TEST_CASE(tcs, many_fields);
    ATF_ADD_TEST_CASE(tcs, escape);
    ATF_ADD_TEST_CASE(tcs, valid_formatters);
    ATF_ADD_TEST_CASE(tcs, bad_format_error);
    ATF_ADD_TEST_CASE(tcs, extra_args_error);
}
