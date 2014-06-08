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

#include "store/migrate.hpp"

#include <fstream>

#include "store/backend.hpp"
#include "store/exceptions.hpp"
#include "store/metadata.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"
#include "utils/sanity.hpp"
#include "utils/stream.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"

namespace fs = utils::fs;
namespace sqlite = utils::sqlite;


namespace {


/// Performs a single migration step.
///
/// \param db Open database to which to apply the migration step.
/// \param version_from Current schema version in the database.
/// \param version_to Schema version to migrate to.
///
/// \throw error If there is a problem applying the migration.
static void
migrate_schema_step(sqlite::database& db, const int version_from,
                    const int version_to)
{
    PRE(version_to == version_from + 1);

    const fs::path migration = store::detail::migration_file(version_from,
                                                             version_to);

    std::ifstream input(migration.c_str());
    if (!input)
        throw store::error(F("Cannot open migration file '%s'") % migration);

    const std::string migration_string = utils::read_stream(input);
    try {
        db.exec(migration_string);
    } catch (const sqlite::error& e) {
        throw store::error(F("Schema migration failed: %s") % e.what());
    }
}


}  // anonymous namespace


/// Calculates the path to a schema migration file.
///
/// \param version_from The version from which the database is being upgraded.
/// \param version_to The version to which the database is being upgraded.
///
/// \return The path to the installed migrate_vX_vY.sql file.
fs::path
store::detail::migration_file(const int version_from, const int version_to)
{
    return fs::path(utils::getenv_with_default("KYUA_STOREDIR", KYUA_STOREDIR))
        / (F("migrate_v%s_v%s.sql") % version_from % version_to);
}


/// Backs up a database for schema migration purposes.
///
/// \todo We should probably use the SQLite backup API instead of doing a raw
/// file copy.  We issue our backup call with the database already open, but
/// because it is quiescent, it's OK to do so.
///
/// \param source Location of the database to be backed up.
/// \param old_version Version of the database's CURRENT schema, used to
///     determine the name of the backup file.
///
/// \throw error If there is a problem during the backup.
void
store::detail::backup_database(const fs::path& source, const int old_version)
{
    const fs::path target(F("%s.v%s.backup") % source.str() % old_version);

    LI(F("Backing up database %s to %s") % source % target);

    std::ifstream input(source.c_str());
    if (!input)
        throw error(F("Cannot open database file %s") % source);

    std::ofstream output(target.c_str());
    if (!output)
        throw error(F("Cannot create database backup file %s") % target);

    char buffer[1024];
    while (input.good()) {
        input.read(buffer, sizeof(buffer));
        if (input.good() || input.eof())
            output.write(buffer, input.gcount());
    }
    if (!input.good() && !input.eof())
        throw error(F("Error while reading input file %s") % source);
}


/// Migrates the schema of a database to the current version.
///
/// The algorithm implemented here performs a migration step for every
/// intermediate version between the schema version in the database to the
/// version implemented in this file.  This should permit upgrades from
/// arbitrary old databases.
///
/// \param file The database whose schema to upgrade.
///
/// \throw error If there is a problem with the migration.
void
store::migrate_schema(const utils::fs::path& file)
{
    sqlite::database db = detail::open_and_setup(file, sqlite::open_readwrite);

    const int version_from = metadata::fetch_latest(db).schema_version();
    const int version_to = detail::current_schema_version;
    if (version_from == version_to) {
        throw error(F("Database already at schema version %s; migration not "
                      "needed") % version_from);
    } else if (version_from > version_to) {
        throw error(F("Database at schema version %s, which is newer than the "
                      "supported version %s") % version_from % version_to);
    }

    detail::backup_database(file, version_from);

    for (int i = version_from; i < version_to; ++i) {
        LI(F("Migrating schema from version %s to %s") % i % (i + 1));
        migrate_schema_step(db, i, i + 1);
    }
}
