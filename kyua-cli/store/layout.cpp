// Copyright 2014 Google Inc.
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

#include "store/layout.hpp"

extern "C" {
#include <dirent.h>
#include <regex.h>
}

#include <algorithm>

#include "store/exceptions.hpp"
#include "utils/datetime.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/fs/operations.hpp"
#include "utils/logging/macros.hpp"
#include "utils/env.hpp"
#include "utils/optional.ipp"
#include "utils/releaser.hpp"
#include "utils/sanity.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace layout = store::layout;

using utils::optional;


namespace {


/// Finds the results file for the latest run of the given test suite.
///
/// \param test_suite Identifier of the test suite to query.
///
/// \return Path to the located database holding the most recent data for the
/// given test suite.
///
/// \throw store::error If no previous results file can be found.
static fs::path
find_latest(const std::string& test_suite)
{
    const fs::path store_dir = layout::query_store_dir();

    ::DIR* dir = ::opendir(store_dir.c_str());
    if (dir == NULL) {
        const int original_errno = errno;
        LW(F("Failed to open store dir %s: %s") % store_dir %
           strerror(original_errno));
        throw store::error(
            F("No previous results file found for test suite %s")
            % test_suite);
    }
    const utils::releaser< ::DIR, int > dir_releaser(dir, ::closedir);

    const std::string pattern = F("^kyua.%s.[0-9]{8}-[0-9]{6}-[0-9]{6}.db$") %
        test_suite;
    ::regex_t preg;
    if (::regcomp(&preg, pattern.c_str(), REG_EXTENDED) != 0)
        throw store::error("Failed to compile regular expression");
    const utils::releaser< ::regex_t, void > preg_releaser(&preg, ::regfree);

    std::string latest;

    ::dirent* de;
    while ((de = ::readdir(dir)) != NULL) {
        ::regmatch_t matches[2];
        const int ret = ::regexec(&preg, de->d_name, 2, matches, 0);
        if (ret == 0) {
            if (latest.empty() || de->d_name > latest) {
                latest = de->d_name;
            }
        } else if (ret == REG_NOMATCH) {
            // Not a database file; skip.
        } else {
            throw store::error("Failed to match regular expression");
        }
    }

    if (latest.empty())
        throw store::error(
            F("No previous results file found for test suite %s")
            % test_suite);

    return store_dir / latest;
}


/// Computes the identifier of a new tests results file.
///
/// \param test_suite Identifier of the test suite.
/// \param when Timestamp to attach to the identifier.
///
/// \return Identifier of the file to be created.
static std::string
new_id(const std::string& test_suite, const datetime::timestamp& when)
{
    const std::string when_datetime = when.strftime("%Y%m%d-%H%M%S");
    const int when_ms = static_cast<int>(when.to_microseconds() % 1000000);
    return F("%s.%s-%06s") % test_suite % when_datetime % when_ms;
}


}  // anonymous namespace


/// Value to request the creation of a new results file with an automatic name.
///
/// Can be passed to new_db().
const char* layout::results_auto_create_name = "NEW";


/// Value to request the opening of the latest results file.
///
/// Can be passed to find_results().
const char* layout::results_auto_open_name = "LATEST";


/// Resolves the results file for the given identifier.
///
/// \param id Identifier of the test suite to open.
///
/// \return Path to the requested file, if any.
///
/// \throw store::error If there is no matching entry.
fs::path
layout::find_results(const std::string& id)
{
    LI(F("Searching for a results file with id %s") % id);

    if (id == results_auto_open_name) {
        const std::string test_suite = test_suite_for_path(fs::current_path());
        return find_latest(test_suite);
    } else {
        const fs::path id_as_path(id);

        if (fs::exists(id_as_path) && !fs::is_directory(id_as_path)) {
            if (id_as_path.is_absolute())
                return id_as_path;
            else
                return id_as_path.to_absolute();
        } else if (id.find('/') == std::string::npos) {
            const fs::path candidate =
                query_store_dir() / (F("kyua.%s.db") % id);
            if (fs::exists(candidate)) {
                return candidate;
            } else {
                return find_latest(id);
            }
        } else {
            INV(id.find('/') != std::string::npos);
            return find_latest(test_suite_for_path(id_as_path));
        }
    }
}


/// Computes the path to a new database for the given test suite.
///
/// \param id Identifier of the test suite to create.
/// \param root Path to the root of the test suite being run, needed to properly
///     autogenerate the identifiers.
///
/// \return Identifier of the created results file, if applicable, and the path
/// to such file.
layout::results_id_file_pair
layout::new_db(const std::string& id, const fs::path& root)
{
    std::string generated_id;
    optional< fs::path > path;

    if (id == results_auto_create_name) {
        generated_id = new_id(test_suite_for_path(root),
                              datetime::timestamp::now());
        path = query_store_dir() / (F("kyua.%s.db") % generated_id);
        fs::mkdir_p(path.get().branch_path(), 0755);
    } else {
        path = fs::path(id);
    }

    return std::make_pair(generated_id, path.get());
}


/// Computes the path to a new database for the given test suite.
///
/// \param id Identifier of the test suite to create.
/// \param root Path to the root of the test suite being run, needed to properly
///     autogenerate the identifiers.
///
/// \return Identifier of the created results file, if applicable, and the path
/// to such file.
fs::path
layout::new_db_for_migration(const fs::path& root,
                             const datetime::timestamp& when)
{
    const std::string generated_id = new_id(test_suite_for_path(root), when);
    const fs::path path = query_store_dir() / (F("kyua.%s.db") % generated_id);
    fs::mkdir_p(path.branch_path(), 0755);
    return path;
}


/// Gets the path to the store directory.
///
/// Note that this function does not create the determined directory.  It is the
/// responsibility of the caller to do so.
///
/// \return Path to the directory holding all the database files.
fs::path
layout::query_store_dir(void)
{
    const optional< fs::path > home = utils::get_home();
    if (home) {
        const fs::path& home_path = home.get();
        if (home_path.is_absolute())
            return home_path / ".kyua/results";
        else
            return home_path.to_absolute() / ".kyua/results";
    } else {
        LW("HOME not defined; creating store database in current "
           "directory");
        return fs::current_path();
    }
}


/// Returns the test suite name for the current directory.
///
/// \return The identifier of the current test suite.
std::string
layout::test_suite_for_path(const fs::path& path)
{
    std::string test_suite;
    if (path.is_absolute())
        test_suite = path.str();
    else
        test_suite = path.to_absolute().str();
    PRE(!test_suite.empty() && test_suite[0] == '/');

    std::replace(test_suite.begin(), test_suite.end(), '/', '_');
    test_suite.erase(0, 1);

    return test_suite;
}
