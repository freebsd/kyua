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

#include "utils/text/operations.ipp"

#include <iostream>
#include <string>
#include <vector>

#include <atf-c++.hpp>

#include "utils/text/exceptions.hpp"

namespace text = utils::text;


namespace {


/// Tests text::refill() on an input string with a range of widths.
///
/// \param expected The expected refilled paragraph.
/// \param input The input paragraph to be refilled.
/// \param first_width The first width to validate.
/// \param last_width The last width to validate (inclusive).
static void
refill_test(const char* expected, const char* input,
            const std::size_t first_width, const std::size_t last_width)
{
    for (std::size_t width = first_width; width <= last_width; ++width) {
        std::cout << "Breaking at width " << width << '\n';
        ATF_REQUIRE_EQ(expected, text::refill(input, width));
    }
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(refill__empty);
ATF_TEST_CASE_BODY(refill__empty)
{
    ATF_REQUIRE_EQ("", text::refill("", 0));
    ATF_REQUIRE_EQ("", text::refill("", 10));
}


ATF_TEST_CASE_WITHOUT_HEAD(refill__no_changes);
ATF_TEST_CASE_BODY(refill__no_changes)
{
    ATF_REQUIRE_EQ("foo  bar\nbaz", text::refill("foo  bar\nbaz", 12));
    ATF_REQUIRE_EQ("foo  bar\nbaz", text::refill("foo  bar\nbaz", 80));
}


ATF_TEST_CASE_WITHOUT_HEAD(refill__break_one);
ATF_TEST_CASE_BODY(refill__break_one)
{
    refill_test("only break the\nfirst line", "only break the first line",
                14, 19);
}


ATF_TEST_CASE_WITHOUT_HEAD(refill__break_one__not_first_word);
ATF_TEST_CASE_BODY(refill__break_one__not_first_word)
{
    refill_test("first-long-word\nother\nwords", "first-long-word other words",
                6, 10);
    refill_test("first-long-word\nother words", "first-long-word other words",
                11, 20);
    refill_test("first-long-word other\nwords", "first-long-word other words",
                21, 26);
    refill_test("first-long-word other words", "first-long-word other words",
                27, 28);
}


ATF_TEST_CASE_WITHOUT_HEAD(refill__break_many);
ATF_TEST_CASE_BODY(refill__break_many)
{
    ATF_REQUIRE_EQ(
        "this is a long\nparagraph to be\nsplit into\npieces",
        text::refill("this is a long paragraph to be split into pieces", 15));
}


ATF_TEST_CASE_WITHOUT_HEAD(refill__preserve_whitespace);
ATF_TEST_CASE_BODY(refill__preserve_whitespace)
{
    ATF_REQUIRE_EQ("foo  bar baz  ", text::refill("foo  bar baz  ", 80));
    ATF_REQUIRE_EQ("foo  \n bar", text::refill("foo    bar", 5));
    ATF_REQUIRE_EQ("foo \n\n bar", text::refill("foo \n  bar", 5));
}


ATF_TEST_CASE_WITHOUT_HEAD(split__empty);
ATF_TEST_CASE_BODY(split__empty)
{
    std::vector< std::string > words = text::split("", ' ');
    std::vector< std::string > exp_words;
    ATF_REQUIRE(exp_words == words);
}


ATF_TEST_CASE_WITHOUT_HEAD(split__one);
ATF_TEST_CASE_BODY(split__one)
{
    std::vector< std::string > words = text::split("foo", ' ');
    std::vector< std::string > exp_words;
    exp_words.push_back("foo");
    ATF_REQUIRE(exp_words == words);
}


ATF_TEST_CASE_WITHOUT_HEAD(split__several__simple);
ATF_TEST_CASE_BODY(split__several__simple)
{
    std::vector< std::string > words = text::split("foo bar baz", ' ');
    std::vector< std::string > exp_words;
    exp_words.push_back("foo");
    exp_words.push_back("bar");
    exp_words.push_back("baz");
    ATF_REQUIRE(exp_words == words);
}


ATF_TEST_CASE_WITHOUT_HEAD(split__several__delimiters);
ATF_TEST_CASE_BODY(split__several__delimiters)
{
    std::vector< std::string > words = text::split("XfooXXbarXXXbazXX", 'X');
    std::vector< std::string > exp_words;
    exp_words.push_back("");
    exp_words.push_back("foo");
    exp_words.push_back("");
    exp_words.push_back("bar");
    exp_words.push_back("");
    exp_words.push_back("");
    exp_words.push_back("baz");
    exp_words.push_back("");
    exp_words.push_back("");
    ATF_REQUIRE(exp_words == words);
}


ATF_TEST_CASE_WITHOUT_HEAD(to_type__ok);
ATF_TEST_CASE_BODY(to_type__ok)
{
    ATF_REQUIRE_EQ(12, text::to_type< int >("12"));
    ATF_REQUIRE_EQ(18745, text::to_type< int >("18745"));
    ATF_REQUIRE_EQ(-12345, text::to_type< int >("-12345"));

    ATF_REQUIRE_EQ(12.0, text::to_type< double >("12"));
    ATF_REQUIRE_EQ(12.5, text::to_type< double >("12.5"));
}


ATF_TEST_CASE_WITHOUT_HEAD(to_type__empty);
ATF_TEST_CASE_BODY(to_type__empty)
{
    ATF_REQUIRE_THROW(text::value_error, text::to_type< int >(""));
}


ATF_TEST_CASE_WITHOUT_HEAD(to_type__invalid);
ATF_TEST_CASE_BODY(to_type__invalid)
{
    ATF_REQUIRE_THROW(text::value_error, text::to_type< int >(" 3"));
    ATF_REQUIRE_THROW(text::value_error, text::to_type< int >("3 "));
    ATF_REQUIRE_THROW(text::value_error, text::to_type< int >("3a"));
    ATF_REQUIRE_THROW(text::value_error, text::to_type< int >("a3"));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, refill__empty);
    ATF_ADD_TEST_CASE(tcs, refill__no_changes);
    ATF_ADD_TEST_CASE(tcs, refill__break_one);
    ATF_ADD_TEST_CASE(tcs, refill__break_one__not_first_word);
    ATF_ADD_TEST_CASE(tcs, refill__break_many);
    ATF_ADD_TEST_CASE(tcs, refill__preserve_whitespace);

    ATF_ADD_TEST_CASE(tcs, split__empty);
    ATF_ADD_TEST_CASE(tcs, split__one);
    ATF_ADD_TEST_CASE(tcs, split__several__simple);
    ATF_ADD_TEST_CASE(tcs, split__several__delimiters);

    ATF_ADD_TEST_CASE(tcs, to_type__ok);
    ATF_ADD_TEST_CASE(tcs, to_type__empty);
    ATF_ADD_TEST_CASE(tcs, to_type__invalid);
}
