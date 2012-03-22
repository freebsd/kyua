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

/// \file utils/text/table.hpp
/// Table construction and formatting.

#if !defined(UTILS_TEXT_TABLE_HPP)
#define UTILS_TEXT_TABLE_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace utils {
namespace text {


/// Values of the cells of a particular table row.
typedef std::vector< std::string > table_row;


/// Representation of a table.
///
/// A table is nothing more than a matrix of rows by columns.  The number of
/// columns is hardcoded at construction times, and the rows can be accumulated
/// at a later stage.
///
/// The only value of this class is a simpler and more natural mechanism of the
/// construction of a table, with additional sanity checks.  We could as well
/// just expose the internal data representation to our users.
class table {
    /// Number of columns in the table.
    table_row::size_type _ncolumns;

    /// Type defining the collection of rows in the table.
    typedef std::vector< table_row > rows_vector;

    /// The rows of the table.
    ///
    /// This is actually the matrix representing the table.  Every element of
    /// this vector (which are vectors themselves) must have _ncolumns items.
    rows_vector _rows;

public:
    table(const table_row::size_type);

    table_row::size_type ncolumns(void) const;

    void add_row(const table_row&);

    bool empty(void) const;

    /// Constant iterator on the rows of the table.
    typedef rows_vector::const_iterator const_iterator;

    const_iterator begin(void) const;
    const_iterator end(void) const;
};


std::vector< std::string > format_table(const table&, const char* = " ",
                                        const std::size_t = 0,
                                        const std::size_t = 0);


}  // namespace text
}  // namespace utils

#endif  // !defined(UTILS_TEXT_TABLE_HPP)
