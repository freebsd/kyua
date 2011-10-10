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

#include <stdexcept>

#include "utils/sanity.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"

namespace sqlite = utils::sqlite;


/// Internal implementation for sqlite::database.
struct utils::sqlite::database::impl {
    /// The SQLite 3 internal database.
    ::sqlite3* db;

    /// Whether we own the database or not (to decide if we close it).
    bool owned;

    /// Constructor.
    ///
    /// \param db_ The SQLite internal database.
    /// \param owned_ Whether this object owns the db_ object or not.  If it
    ///     does, the internal db_ will be released during destruction.
    impl(::sqlite3* db_, const bool owned_) :
        db(db_),
        owned(owned_)
    {
    }
};


/// Initializes the SQLite database.
///
/// You must share the same database object alongside the lifetime of your
/// SQLite session.  As soon as the object is destroyed, the session is
/// terminated.
sqlite::database::database(void* db_, const bool owned_) :
    _pimpl(new impl(static_cast< ::sqlite3* >(db_), owned_))
{
}


/// Destructor for the SQLite 3 database.
///
/// Closes the session unless it has already been closed by calling the
/// close() method.  It is recommended to explicitly close the session in the
/// code.
sqlite::database::~database(void)
{
    if (_pimpl->owned && _pimpl->db != NULL)
        close();
}


/// Opens an SQLite database.
///
/// \param file The path to the database file to be opened.  This follows the
///     same conventions as the filename passed to the C library: i.e. the
///     names "" and ":memory:" are valid and recognized.
/// \param open_flags The flags to be passed to the open routine.
///
/// \throw std::bad_alloc If there is not enough memory to open the database.
/// \throw api_error If there is any problem opening the database.
sqlite::database
sqlite::database::open(const fs::path& file, int open_flags)
{
    int flags = 0;
    if (open_flags & open_readonly) {
        flags |= SQLITE_OPEN_READONLY;
        open_flags &= ~open_readonly;
    }
    if (open_flags & open_readwrite) {
        flags |= SQLITE_OPEN_READWRITE;
        open_flags &= ~open_readwrite;
    }
    if (open_flags & open_create) {
        flags |= SQLITE_OPEN_CREATE;
        open_flags &= ~open_create;
    }
    PRE(open_flags == 0);

    ::sqlite3* db;
    const int error = ::sqlite3_open_v2(file.c_str(), &db, flags, NULL);
    if (error != SQLITE_OK) {
        if (db == NULL)
            throw std::bad_alloc();
        else {
            database error_db(db, true);
            throw sqlite::api_error::from_database(error_db, "sqlite3_open_v2");
        }
    }
    INV(db != NULL);
    return database(db, true);
}


/// Gets the internal ::sqlite3 object.
///
/// \return The raw SQLite 3 database.  This is returned as a void pointer to
/// prevent including the sqlite3.h header file from our public interface.  The
/// only way to call this method is by using the c_gate module, and c_gate takes
/// care of casting this object to the appropriate type.
void*
sqlite::database::raw_database(void)
{
    return _pimpl->db;
}


/// Terminates the connection to the database.
///
/// It is recommended to call this instead of relying on the destructor to do
/// the cleanup, but it is not a requirement to use close().
///
/// \pre close() has not yet been called.
void
sqlite::database::close(void)
{
    PRE(_pimpl->db != NULL);
    int error = ::sqlite3_close(_pimpl->db);
    // For now, let's consider a return of SQLITE_BUSY an error.  We should not
    // be trying to close a busy database in our code.  Maybe revisit this later
    // to raise busy errors as exceptions.
    PRE(error == SQLITE_OK);
    _pimpl->db = NULL;
}


/// Executes an arbitrary SQL string.
///
/// As the documentation explains, this is unsafe.  The code should really be
/// preparing statements and executing them step by step.  However, it is
/// perfectly fine to use this function for, e.g. the initial creation of
/// tables in a database and in tests.
///
/// \param sql The SQL commands to be executed.
///
/// \throw api_error If there is any problem while processing the SQL.
void
sqlite::database::exec(const std::string& sql)
{
    const int error = ::sqlite3_exec(_pimpl->db, sql.c_str(), NULL, NULL, NULL);
    if (error != SQLITE_OK)
        throw api_error::from_database(*this, "sqlite3_exec");
}
