// Copyright 2011 Google Inc.
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

extern "C" {
#include <sqlite3.h>
}

#include "utils/defs.hpp"
#include "utils/sanity.hpp"
#include "utils/sqlite/c_gate.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.hpp"

namespace sqlite = utils::sqlite;


namespace {


static sqlite::type c_type_to_cxx(const int) UTILS_PURE;


/// Maps a SQLite 3 data type to our own representation.
///
/// \param original The native SQLite 3 data type.
///
/// \return Our internal representation for the native data type.
static sqlite::type
c_type_to_cxx(const int original)
{
    switch (original) {
    case SQLITE_BLOB: return sqlite::type_blob;
    case SQLITE_FLOAT: return sqlite::type_float;
    case SQLITE_INTEGER: return sqlite::type_integer;
    case SQLITE_NULL: return sqlite::type_null;
    case SQLITE_TEXT: return sqlite::type_text;
    default: UNREACHABLE_MSG("Unknown data type returned by SQLite 3");
    }
    UNREACHABLE;
}


}  // anonymous namespace


/// Internal implementation for sqlite::statement.
struct utils::sqlite::statement::impl {
    /// The database this statement belongs to.
    sqlite::database& db;

    /// The SQLite 3 internal statement.
    ::sqlite3_stmt* stmt;

    /// Constructor.
    ///
    /// \param db_ The database this statement belongs to.  Be aware that we
    ///     keep a *reference* to the database; in other words, if the database
    ///     vanishes, this object will become invalid.  (It'd be trivial to keep
    ///     a shallow copy here instead, but I feel that statements that outlive
    ///     their database represents sloppy programming.)
    /// \param stmt_ The SQLite internal statement.
    impl(database& db_, ::sqlite3_stmt* stmt_) :
        db(db_),
        stmt(stmt_)
    {
    }

    /// Destructor.
    ///
    /// It is important to keep this as part of the 'impl' class instead of the
    /// container class.  The 'impl' class is destroyed exactly once (because it
    /// is managed by a shared_ptr) and thus releasing the resources here is
    /// OK.  However, the container class is potentially released many times,
    /// which means that we would be double-freeing the internal object and
    /// reusing invalid data.
    ~impl(void)
    {
        (void)::sqlite3_finalize(stmt);
    }
};


/// Initializes a statement object.
///
/// This is an internal function.  Use database::create_statement() to
/// instantiate one of these objects.
///
/// \param db The database this statement belongs to.
/// \param raw_stmt A void pointer representing a SQLite native statement of
///     type ::sqlite3_stmt.
sqlite::statement::statement(database& db, void* raw_stmt) :
    _pimpl(new impl(db, static_cast< ::sqlite3_stmt* >(raw_stmt)))
{
}


/// Destructor for the statement.
///
/// Remember that statements are reference-counted, so the statement will only
/// cease to be valid once its last copy is destroyed.
sqlite::statement::~statement(void)
{
}


/// Performs a processing step on the statement.
///
/// \return True if the statement returned a row; false if the processing has
/// finished.
///
/// \throw api_error If the processing of the step raises an error.
bool
sqlite::statement::step(void)
{
    const int error = ::sqlite3_step(_pimpl->stmt);
    switch (error) {
    case SQLITE_DONE:
        return false;
    case SQLITE_ROW:
        return true;
    default:
        throw api_error::from_database(_pimpl->db, "sqlite3_step");
    }
    UNREACHABLE;
}


/// Returns the number of columns in the step result.
///
/// \return The number of columns available for data retrieval.
int
sqlite::statement::column_count(void)
{
    return ::sqlite3_column_count(_pimpl->stmt);
}


/// Returns the name of a particular column in the result.
///
/// \param index The column to request the name of.
///
/// \return The name of the requested column.
std::string
sqlite::statement::column_name(const int index)
{
    const char* name = ::sqlite3_column_name(_pimpl->stmt, index);
    if (name == NULL)
        throw api_error::from_database(_pimpl->db, "sqlite3_column_name");
    return name;
}


/// Returns the type of a particular column in the result.
///
/// \param index The column to request the type of.
///
/// \return The type of the requested column.
sqlite::type
sqlite::statement::column_type(const int index)
{
    return c_type_to_cxx(::sqlite3_column_type(_pimpl->stmt, index));
}


/// Returns a particular column in the result as a blob.
///
/// \param index The column to retrieve.
///
/// \return A block of memory with the blob contents.  Note that the pointer
/// returned by this call will be invalidated on the next call to any SQLite API
/// function.
const void*
sqlite::statement::column_blob(const int index)
{
    PRE(column_type(index) == type_blob);
    return ::sqlite3_column_blob(_pimpl->stmt, index);
}


/// Returns a particular column in the result as a double.
///
/// \param index The column to retrieve.
///
/// \return The double value.
double
sqlite::statement::column_double(const int index)
{
    PRE(column_type(index) == type_float);
    return ::sqlite3_column_double(_pimpl->stmt, index);
}


/// Returns a particular column in the result as an integer.
///
/// \param index The column to retrieve.
///
/// \return The integer value.  Note that the value may not fit in an integer
/// depending on the platform.  Use column_int64 to retrieve the integer without
/// truncation.
int
sqlite::statement::column_int(const int index)
{
    PRE(column_type(index) == type_integer);
    return ::sqlite3_column_int(_pimpl->stmt, index);
}


/// Returns a particular column in the result as a 64-bit integer.
///
/// \param index The column to retrieve.
///
/// \return The integer value.
int64_t
sqlite::statement::column_int64(const int index)
{
    PRE(column_type(index) == type_integer);
    return ::sqlite3_column_int64(_pimpl->stmt, index);
}


/// Returns a particular column in the result as a double.
///
/// \param index The column to retrieve.
///
/// \return A C string with the contents.  Note that the pointer returned by
/// this call will be invalidated on the next call to any SQLite API function.
/// If you want to be extra safe, store the result in a std::string to not worry
/// about this.
const char*
sqlite::statement::column_text(const int index)
{
    PRE(column_type(index) == type_text);
    return reinterpret_cast< const char* >(::sqlite3_column_text(
        _pimpl->stmt, index));
}


/// Returns the number of bytes stored in the column.
///
/// \pre This is only valid for columns of type blob and text.
///
/// \param index The column to retrieve the size of.
///
/// \return The number of bytes in the column.  Remember that strings are stored
/// in their UTF-8 representation; this call returns the number of *bytes*, not
/// characters.
int
sqlite::statement::column_bytes(const int index)
{
    PRE(column_type(index) == type_blob || column_type(index) == type_text);
    return ::sqlite3_column_bytes(_pimpl->stmt, index);
}


/// Resets a statement to allow further processing.
void
sqlite::statement::reset(void)
{
    (void)::sqlite3_reset(_pimpl->stmt);
}
