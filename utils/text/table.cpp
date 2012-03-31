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

#include <algorithm>
#include <iterator>
#include <limits>
#include <sstream>

#include "utils/sanity.hpp"
#include "utils/text/operations.ipp"

namespace text = utils::text;


namespace {


/// Colletion of widths of the columns of a table.
typedef std::vector< std::size_t > widths_vector;


/// Calculates the maximum widths of the columns of a table.
///
/// \param table The table from which to calculate the column widths.
///
/// \return A vector with the widths of the columns of the input table.
static widths_vector
column_widths(const text::table& table)
{
    widths_vector widths(table.ncolumns(), 0);

    for (text::table::const_iterator iter = table.begin(); iter != table.end();
         ++iter) {
        const text::table_row& row = *iter;
        INV(row.size() == table.ncolumns());
        for (text::table_row::size_type i = 0; i < row.size(); ++i)
            if (widths[i] < row[i].size())
                widths[i] = row[i].size();
    }

    return widths;
}


/// Adjust the width of the refillable column.
///
/// \param [in,out] widths The current widths of the table columns.
///     widths[expanding_column] is updated on exit.
/// \param max_width_of_table The maximum width that the table can have.  Must
///     be non-zero.
/// \param expanding_column The column to be refilled when the table does not
///     fit in the specified width.
/// \param extra Extra width used by the caller when formatting the table.  This
///     is used to account for the cells separator, if any.
static void
adjust_widths(widths_vector& widths, const std::size_t max_width_of_table,
              const std::size_t expanding_column,
              const std::size_t extra)
{
    PRE(max_width_of_table > 0);

    std::string::size_type width = 0;
    for (widths_vector::size_type i = 0; i < widths.size(); ++i) {
        if (i != expanding_column)
            width += widths[i];
    }
    widths[expanding_column] = max_width_of_table - width - extra;
}


/// Pads an input text to a specified width with spaces.
///
/// \param input The text to add padding to (may be empty).
/// \param length The desired length of the output.
/// \param is_last Whether the text being processed belongs to the last column
///     of a row or not.  Values in the last column should not be padded to
///     prevent trailing whitespace on the screen (which affects copy/pasting
///     for example).
///
/// \return The padded cell.  If the input string is longer than the desired
/// length, the input string is returned verbatim.  The padded table won't be
/// correct, but we don't expect this to be a common case to worry about.
static std::string
pad_cell(const std::string& input, const std::size_t length, const bool is_last)
{
    if (is_last)
        return input;
    else {
        if (input.length() < length)
            return input + std::string(length - input.length(), ' ');
        else
            return input;
    }
}


/// Refills a cell and adds it to the output lines.
///
/// \param row The row containing the cell to be refilled.
/// \param widths The widths of the row.
/// \param column The column being refilled.
/// \param [in,out] textual_rows The output lines as processed so far.  This is
///     updated to accomodate for the contents of the refilled cell, extending
///     the rows as necessary.
static void
refill_cell(const text::table_row& row, const widths_vector& widths,
            const text::table_row::size_type column,
            std::vector< text::table_row >& textual_rows)
{
    const std::vector< std::string > rows = text::refill(row[column],
                                                         widths[column]);

    if (textual_rows.size() < rows.size())
        textual_rows.resize(rows.size(), text::table_row(row.size()));

    for (std::vector< std::string >::size_type i = 0; i < rows.size(); ++i) {
        for (text::table_row::size_type j = 0; j < row.size(); ++j) {
            const bool is_last = j == row.size() - 1;
            if (j == column)
                textual_rows[i][j] = pad_cell(rows[i], widths[j], is_last);
            else {
                if (textual_rows[i][j].empty())
                    textual_rows[i][j] = pad_cell("", widths[j], is_last);
            }
        }
    }
}


/// Formats a single table row.
///
/// \param row The row to format.
/// \param widths The widths of the columns to apply during formatting.  Cells
///     wider than the specified width are refilled to attempt to fit in the
///     cell.  Cells narrower than the width are right-padded with spaces.
/// \param separator The column separator to use.
///
/// \return The textual lines that contain the formatted row.
static std::vector< std::string >
format_row(const text::table_row& row, const widths_vector& widths,
           const std::string& separator)
{
    PRE(row.size() == widths.size());

    std::vector< text::table_row > textual_rows(1, text::table_row(row.size()));

    for (text::table_row::size_type column = 0; column < row.size(); ++column) {
        if (widths[column] > row[column].length())
            textual_rows[0][column] = pad_cell(row[column], widths[column],
                                               column == row.size() - 1);
        else
            refill_cell(row, widths, column, textual_rows);
    }

    std::vector< std::string > lines;
    for (std::vector< text::table_row >::const_iterator
         iter = textual_rows.begin(); iter != textual_rows.end(); ++iter) {
        lines.push_back(text::join(*iter, separator));
    }
    return lines;
}


}  // anonymous namespace


/// Constructs a new table.
///
/// \param ncolumns_ The number of columns that the table will have.
text::table::table(const table_row::size_type ncolumns_) :
    _ncolumns(ncolumns_)
{
}


/// Gets the number of columns in the table.
///
/// \return The number of columns in the table.  This value remains constant
/// during the existence of the table.
text::table_row::size_type
text::table::ncolumns(void) const
{
    return _ncolumns;
}


/// Checks whether the table is empty or not.
///
/// \return True if the table is empty; false otherwise.
bool
text::table::empty(void) const
{
    return _rows.empty();
}


/// Adds a row to the table.
///
/// \param row The row to be added.  This row must have the same amount of
///     columns as defined during the construction of the table.
void
text::table::add_row(const table_row& row)
{
    PRE(row.size() == _ncolumns);
    _rows.push_back(row);
}


/// Gets an iterator pointing to the beginning of the rows of the table.
///
/// \return An iterator on the rows.
text::table::const_iterator
text::table::begin(void) const
{
    return _rows.begin();
}


/// Gets an iterator pointing to the end of the rows of the table.
///
/// \return An iterator on the rows.
text::table::const_iterator
text::table::end(void) const
{
    return _rows.end();
}


/// Column width to denote that the column can be refilled to fit the table.
const std::size_t text::table_formatter::width_refill =
    std::numeric_limits< std::size_t >::max();


/// Constructs a new table formatter.
text::table_formatter::table_formatter(void) :
    _separator(""),
    _table_width(0),
    _refill_column(0)
{
}


/// Sets the width of a column.
///
/// All columns except one must have a width that is, at least, as wide as the
/// widest cell in the column.  One of the columns can have a width of
/// width_refill, which indicates that the column will be refilled if the table
/// does not fit in its maximum width.
///
/// \param column The index of the column to set the width for.
/// \param width The width to set the column to.
///
/// \return A reference to this formatter to allow using the builder pattern.
text::table_formatter&
text::table_formatter::set_column_width(const table_row::size_type column,
                                        const std::size_t width)
{
    if (width == width_refill)
        _refill_column = column;
    else
        UNREACHABLE_MSG("Not yet implemented");
    return *this;
}


/// Sets the separator to use between the cells.
///
/// \param separator The separator to use.
///
/// \return A reference to this formatter to allow using the builder pattern.
text::table_formatter&
text::table_formatter::set_separator(const char* separator)
{
    _separator = separator;
    return *this;
}


/// Sets the maximum width of the table.
///
/// \param table_width The maximum width of the table; cannot be zero.
///
/// \return A reference to this formatter to allow using the builder pattern.
text::table_formatter&
text::table_formatter::set_table_width(const std::size_t table_width)
{
    PRE(table_width > 0);
    _table_width = table_width;
    return *this;
}


/// Formats a table into a collection of textual lines.
///
/// \param t Table to format.
///
/// \return A collection of textual lines.
std::vector< std::string >
text::table_formatter::format(const table& t) const
{
    std::vector< std::string > lines;

    if (!t.empty()) {
        widths_vector widths = column_widths(t);
        if (_table_width > 0)
            adjust_widths(widths, _table_width, _refill_column,
                          (t.ncolumns() - 1) * _separator.length());

        for (table::const_iterator iter = t.begin(); iter != t.end(); ++iter) {
            const std::vector< std::string > sublines =
                format_row(*iter, widths, _separator);
            std::copy(sublines.begin(), sublines.end(),
                      std::back_inserter(lines));
        }
    }

    return lines;
}
