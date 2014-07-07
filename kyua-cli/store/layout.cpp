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


/// Finds the database corresponding to the latest run for the given test suite.
///
/// \param test_suite Identifier of the test suite to query.
///
/// \return Path to the located database holding the most recent data for the
/// given test suite.
///
/// \raises store::error If no previous action can be found.
fs::path
layout::find_latest(const std::string& test_suite)
{
    const fs::path store_dir = query_store_dir();

    ::DIR* dir = ::opendir(store_dir.c_str());
    if (dir == NULL) {
        const int original_errno = errno;
        LW(F("Failed to open store dir %s: %s") % store_dir %
           strerror(original_errno));
        throw store::error(F("No previous action found for test suite %s") %
                           test_suite);
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
        throw store::error(F("No previous action found for test suite %s") %
                           test_suite);

    return store_dir / latest;
}


/// Computes the path to a new database for the given test suite.
///
/// \param test_suite Identifier of the test suite to create.
///
/// \return Path to the newly-determined path for the database to be created.
///
/// \raises store::error If the computed name already exists; this should not
///     happen.
fs::path
layout::new_db(const std::string& test_suite)
{
    const datetime::timestamp now = datetime::timestamp::now();
    const std::string now_datetime = now.strftime("%Y%m%d-%H%M%S");
    const int now_ms = static_cast<int>(now.to_microseconds() % 1000000);
    const fs::path path = query_store_dir() /
        (F("kyua.%s.%s-%06s.db") % test_suite % now_datetime % now_ms);
    if (fs::exists(path))
        throw store::error("Computed test suite store already exists");
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
            return home_path / ".kyua/actions";
        else
            return home_path.to_absolute() / ".kyua/actions";
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
