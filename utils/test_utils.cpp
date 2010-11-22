// Copyright 2010, Google Inc.
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
#include <regex.h>
#include <unistd.h>
}

#include <atf-c++.hpp>

#include <fstream>
#include <iostream>
#include <string>

#include "utils/format/macros.hpp"
#include "utils/test_utils.hpp"

namespace fs = utils::fs;


/// Dumps the contents of a file on the standard output.
///
/// \param prefix A string to use as a prefix for all the printed lines.  May be
///     empty.
/// \param path The path to the file to print.
void
utils::cat_file(const std::string& prefix, const fs::path& path)
{
    std::ifstream input(path.c_str());
    if (!input)
        ATF_FAIL(F("Cannot open file %s") % path);

    std::string line;
    while (std::getline(input, line).good()) {
        std::cout << prefix << line << "\n";
    }
}


/// Checks if a file exists.
///
/// Be aware that this is racy in the same way as access(2) is.
///
/// \param path The file to check the existance of.
///
/// \return True if the file exists; false otherwise.
bool
utils::exists(const fs::path& path)
{
    return ::access(path.c_str(), F_OK) == 0;
}


/// Looks for a regular expression in a file.
///
/// \param regexp The regular expression.
/// \param path The path to the file to query.
///
/// \return True if the regular expression matches anywhere in the file; false
/// otherwise.
bool
utils::grep_file(const std::string& regexp, const fs::path& path)
{
    std::ifstream input(path.c_str());
    if (!input)
        ATF_FAIL(F("Cannot open file %s") % path);

    std::string line;
    while (std::getline(input, line).good()) {
        if (grep_string(regexp, line))
            return true;
    }
    return false;
}


/// Looks for a regular expression in a string.
///
/// \param regexp The regular expression.
/// \param path The string to query.
///
/// \return True if the regular expression matches anywhere in the string; false
/// otherwise.
bool
utils::grep_string(const std::string& regexp, const std::string& str)
{
    regex_t preg;
    ATF_REQUIRE(::regcomp(&preg, regexp.c_str(), REG_EXTENDED) == 0);
    const int res = ::regexec(&preg, str.c_str(), 0, NULL, 0);
    ATF_REQUIRE(res == 0 || res == REG_NOMATCH);
    ::regfree(&preg);
    return res == 0;
}


/// Looks for a regular expression in a vector of strings.
///
/// \param regexp The regular expression.
/// \param path The vector to query.
///
/// \return True if the regular expression matches anywhere in the vector; false
/// otherwise.
bool
utils::grep_vector(const std::string& regexp,
                   const std::vector< std::string >& v)
{
    for (std::vector< std::string >::const_iterator iter = v.begin();
         iter != v.end(); iter++) {
        if (grep_string(regexp, *iter))
            return true;
    }
    return false;
}
