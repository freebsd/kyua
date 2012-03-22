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

#include "utils/text/table.hpp"

#include <atf-c++.hpp>

#include "utils/text/operations.ipp"

namespace text = utils::text;


/// Performs a check on text::format_table().
///
/// This is provided for simplicity reasons only.  This is a macro instead of a
/// function because we want an easy way to delegate the multiple parameters of
/// text::format_table() verbatim, regardless of whether its optional values
/// have been supplied or not.
///
/// Because of the flattening of the formatted table into a string, we risk
/// misdetecting problems when the algorithm bundles newlines into the lines of
/// a table.  This should not happen, and not accounting for this little detail
/// makes testing so much easier.
///
/// \param expected Textual representation of the table, as a collection of
///     lines separated by newline characters.
/// \param arguments The arguments to pass to text::format_table().  These have
///     to be provided verbatim, including the surrounding parenthesis.  Note
///     that the table to be formatted must not be empty.
#define FORMAT_TABLE_CHECK(expected, arguments) \
    ATF_REQUIRE_EQ(expected, \
                   text::join(text::format_table arguments, "\n") + "\n")


ATF_TEST_CASE_WITHOUT_HEAD(table__ncolumns);
ATF_TEST_CASE_BODY(table__ncolumns)
{
    ATF_REQUIRE_EQ(5, text::table(5).ncolumns());
    ATF_REQUIRE_EQ(10, text::table(10).ncolumns());
}


ATF_TEST_CASE_WITHOUT_HEAD(table__empty);
ATF_TEST_CASE_BODY(table__empty)
{
    text::table table(2);
    ATF_REQUIRE(table.empty());
    table.add_row(text::table_row(2));
    ATF_REQUIRE(!table.empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(table__iterate);
ATF_TEST_CASE_BODY(table__iterate)
{
    text::table_row row1;
    row1.push_back("foo");
    text::table_row row2;
    row2.push_back("bar");

    text::table table(1);
    table.add_row(row1);
    table.add_row(row2);

    text::table::const_iterator iter = table.begin();
    ATF_REQUIRE(iter != table.end());
    ATF_REQUIRE(row1 == *iter);
    ++iter;
    ATF_REQUIRE(iter != table.end());
    ATF_REQUIRE(row2 == *iter);
    ++iter;
    ATF_REQUIRE(iter == table.end());
}


ATF_TEST_CASE_WITHOUT_HEAD(format_table__empty);
ATF_TEST_CASE_BODY(format_table__empty)
{
    ATF_REQUIRE(text::format_table(text::table(1), " ").empty());
    ATF_REQUIRE(text::format_table(text::table(10), " ").empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(format_table__one_column__no_refill);
ATF_TEST_CASE_BODY(format_table__one_column__no_refill)
{
    text::table table(1);
    {
        text::table_row row;
        row.push_back("First row with some words");
        table.add_row(row);
    }
    {
        text::table_row row;
        row.push_back("Second row with some words");
        table.add_row(row);
    }

    FORMAT_TABLE_CHECK(
        "First row with some words\n"
        "Second row with some words\n",
        (table, " | ", 0, 0));
}


ATF_TEST_CASE_WITHOUT_HEAD(format_table__one_column__refill);
ATF_TEST_CASE_BODY(format_table__one_column__refill)
{
    text::table table(1);
    {
        text::table_row row;
        row.push_back("First row with some words");
        table.add_row(row);
    }
    {
        text::table_row row;
        row.push_back("Second row with some words");
        table.add_row(row);
    }

    FORMAT_TABLE_CHECK(
        "First row\nwith some\nwords\n"
        "Second row\nwith some\nwords\n",
        (table, " | ", 11, 0));
}


ATF_TEST_CASE_WITHOUT_HEAD(format_table__many_columns__no_refill);
ATF_TEST_CASE_BODY(format_table__many_columns__no_refill)
{
    text::table table(3);
    {
        text::table_row row;
        row.push_back("First");
        row.push_back("Second");
        row.push_back("Third");
        table.add_row(row);
    }
    {
        text::table_row row;
        row.push_back("Fourth with some text");
        row.push_back("Fifth with some more text");
        row.push_back("Sixth foo");
        table.add_row(row);
    }

    FORMAT_TABLE_CHECK(
        "First                 | Second                    | Third\n"
        "Fourth with some text | Fifth with some more text | Sixth foo\n",
        (table, " | "));
}


ATF_TEST_CASE_WITHOUT_HEAD(format_table__many_columns__refill);
ATF_TEST_CASE_BODY(format_table__many_columns__refill)
{
    text::table table(3);
    {
        text::table_row row;
        row.push_back("First");
        row.push_back("Second");
        row.push_back("Third");
        table.add_row(row);
    }
    {
        text::table_row row;
        row.push_back("Fourth with some text");
        row.push_back("Fifth with some more text");
        row.push_back("Sixth foo");
        table.add_row(row);
    }

    FORMAT_TABLE_CHECK(
        "First                 | Second     | Third\n"
        "Fourth with some text | Fifth with | Sixth foo\n"
        "                      | some more  | \n"
        "                      | text       | \n",
        (table, " | ", 46, 1));
}


ATF_TEST_CASE_WITHOUT_HEAD(format_table__use_case__cli_help);
ATF_TEST_CASE_BODY(format_table__use_case__cli_help)
{
    text::table table(2);
    {
        text::table_row row;
        row.push_back("-a a_value");
        row.push_back("This is the description of the first flag");
        table.add_row(row);
    }
    {
        text::table_row row;
        row.push_back("-b");
        row.push_back("And this is the text for the second flag");
        table.add_row(row);
    }

    FORMAT_TABLE_CHECK(
        "-a a_value  This is the description\n"
        "            of the first flag\n"
        "-b          And this is the text for\n"
        "            the second flag\n",
        (table, "  ", 36, 1));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, table__ncolumns);
    ATF_ADD_TEST_CASE(tcs, table__empty);
    ATF_ADD_TEST_CASE(tcs, table__iterate);

    ATF_ADD_TEST_CASE(tcs, format_table__empty);
    ATF_ADD_TEST_CASE(tcs, format_table__one_column__no_refill);
    ATF_ADD_TEST_CASE(tcs, format_table__one_column__refill);
    ATF_ADD_TEST_CASE(tcs, format_table__many_columns__no_refill);
    ATF_ADD_TEST_CASE(tcs, format_table__many_columns__refill);
    ATF_ADD_TEST_CASE(tcs, format_table__use_case__cli_help);
}
