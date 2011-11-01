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

#include <fstream>

#include "store/backend.hpp"
#include "store/exceptions.hpp"
#include "store/metadata.hpp"
#include "store/transaction.hpp"
#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"
#include "utils/sanity.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.hpp"

namespace fs = utils::fs;
namespace sqlite = utils::sqlite;


/// The current schema version.
///
/// Any new database gets this schema version.  Existing databases with an older
/// schema version must be first migrated to the current schema before they can
/// be used.
///
/// This must be kept in sync with the value in schema.sql.
const int store::detail::current_schema_version = 1;


namespace {


/// Gets the length of a stream.
///
/// \param is The input stream for which to calculate its length.
///
/// \return The length of the stream.  If calculating the length fails, this
/// returns 0 instead of raising an exception.
static std::streampos
stream_length(std::istream& is)
{
    const std::streampos current_pos = is.tellg();
    try {
        is.seekg(0, std::ios::end);
        const std::streampos length = is.tellg();
        is.seekg(current_pos, std::ios::beg);
        return length;
    } catch (...) {
        is.seekg(current_pos, std::ios::beg);
        LW("Failed to calculate stream length");
        return 0;
    }
}


/// Reads the whole contents of a stream into memory.
///
/// \param is The input stream from which to read.
///
/// \return A plain string containing the raw contents of the file.
static std::string
read_file(std::istream& is)
{
    std::string buffer;
    buffer.reserve(stream_length(is));

    char part[1024];
    while (is.good()) {
        is.read(part, sizeof(part) - 1);
        INV(static_cast< unsigned long >(is.gcount()) < sizeof(part));
        part[is.gcount()] = '\0';
        buffer += part;
    }

    return buffer;
}


/// Opens a database and defines session pragmas.
///
/// This auxiliary function ensures that, every time we open a SQLite database,
/// we define the same set of pragmas for it.
///
/// \param file The database file to be opened.
/// \param flags The flags for the open; see sqlite::database::open.
///
/// \return The opened database.
///
/// \throw store::error If there is a problem opening or creating the database.
static sqlite::database
do_open(const fs::path& file, const int flags)
{
    try {
        sqlite::database database = sqlite::database::open(file, flags);
        database.exec("PRAGMA foreign_keys = ON");
        return database;
    } catch (const sqlite::error& e) {
        throw store::error(F("Cannot open '%s': %s") % e.what() % file);
    }
}


/// Checks if a database is empty (i.e. if it is new).
///
/// \param db The database to check. 
///
/// \return True if the database is empty.
static bool
empty_database(sqlite::database& db)
{
    sqlite::statement stmt = db.create_statement("SELECT * FROM sqlite_master");
    return !stmt.step();
}


}  // anonymous namespace


/// The path to the schema file to be used by initialize().
const fs::path store::detail::schema_file =
    fs::path(KYUA_STOREDIR) / "schema.sql";


/// Initializes an empty database.
///
/// \param db The database to initialize.
/// \param file The schema file to use; for testing purposes.
///
/// \return The metadata record written into the new database.
///
/// \throw store::error If there is a problem initializing the database.
store::metadata
store::detail::initialize(sqlite::database& db, const fs::path& file)
{
    PRE(empty_database(db));

    std::ifstream input(file.c_str());
    if (!input)
        throw error(F("Cannot open database schema '%s'") % file);

    LI(F("Populating new database with schema from %s") % file);
    const std::string schema_string = read_file(input);
    try {
        db.exec(schema_string);

        const metadata metadata = metadata::fetch_latest(db);
        LI(F("New metadata entry %d") % metadata.timestamp());
        if (metadata.schema_version() != detail::current_schema_version) {
            UNREACHABLE_MSG("current_schema_version is out of sync with "
                            "schema.sql");
        }
        return metadata;
    } catch (const store::integrity_error& e) {
        // Could be raised by metadata::fetch_latest.
        UNREACHABLE_MSG("Inconsistent code while creating a database");
    } catch (const sqlite::error& e) {
        throw error(F("Failed to initialize database: %s") % e.what());
    }
}


/// Internal implementation for the backend.
struct store::backend::impl {
    /// The SQLite database this backend talks to.
    sqlite::database database;

    /// Constructor.
    ///
    /// \param database_ The SQLite database instance.
    /// \param metadata_ The metadata for the loaded database.  This must match
    ///     the schema version we implement in this module; otherwise, a
    ///     migration is necessary.
    impl(sqlite::database& database_, const metadata& metadata_) :
        database(database_)
    {
        if (metadata_.schema_version() != detail::current_schema_version)
            throw integrity_error(F("Found schema version %d in database but "
                                    "this version does not exist") %
                                  metadata_.schema_version());
    }
};


/// Constructs a new backend.
///
/// \param pimpl_ The internal data.
store::backend::backend(impl* pimpl_) :
    _pimpl(pimpl_)
{
}


/// Destructor.
store::backend::~backend(void)
{
}


/// Opens a database in read-only mode.
///
/// \param file The database file to be opened.
///
/// \return The backend representation.
///
/// \throw store::error If there is any problem opening the database.
store::backend
store::backend::open_ro(const fs::path& file)
{
    sqlite::database db = do_open(file, sqlite::open_readonly);
    return backend(new impl(db, metadata::fetch_latest(db)));
}


/// Opens a database in read-write mode and creates it if necessary.
///
/// \param file The database file to be opened.
///
/// \return The backend representation.
///
/// \throw store::error If there is any problem opening or creating
///     the database.
store::backend
store::backend::open_rw(const fs::path& file)
{
    sqlite::database db = do_open(file, sqlite::open_readwrite |
                                  sqlite::open_create);
    if (empty_database(db))
        return backend(new impl(db, detail::initialize(db)));
    else
        return backend(new impl(db, metadata::fetch_latest(db)));
}


/// Gets the connection to the SQLite database.
///
/// \return A database connection.
sqlite::database&
store::backend::database(void)
{
    return _pimpl->database;
}


/// Opens a transaction.
///
/// \return A new transaction.
store::transaction
store::backend::start(void)
{
    return transaction(*this);
}
