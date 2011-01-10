// Copyright 2010, 2011 Google Inc.
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
#include <iostream>

#include "engine/exceptions.hpp"
#include "engine/kyuafile.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"

namespace cmdline = utils::cmdline;
namespace fs = utils::fs;

// TODO(jmmv): Replace this adhoc trivial parser with a full parser.  Use
// either Yacc or Bison.  Don't bother reimplementing a parser: automake
// ships the generated files by default, so we can assume end users don't
// need to deal with tool incompatibilities.


namespace {


/// Parses a single test suite configuration file.
///
/// This is a recursive algorithm to load configuration files with inclusions.
/// It is just a helper function for the kyuafile::load() method.
///
/// \param suite The file to parse.
/// \param config_file The directory in which the file is located, relative to
///     the initial of the recursive load.
/// \param test_programs [out] Accumulator that absorbs the collection of test
///     programs executable names as they are found.
///
/// \throw error If suite does not exist.  TODO(jmmv): This exception is not
///     accurate enough.
static void
load_one(const utils::fs::path& suite, const utils::fs::path& directory,
         std::vector< utils::fs::path >& test_programs)
{
    std::ifstream is(suite.c_str());
    if (!is)
        throw engine::error(F("Failed to open %s") % suite);

    std::string line;
    while (std::getline(is, line).good()) {
        if (line.substr(0, 8) == "include ") {
            const utils::fs::path include(line.substr(8));
            if (directory != utils::fs::path("."))
                load_one(directory / include, directory / include.branch_path(),
                         test_programs);
            else
                load_one(include, include.branch_path(), test_programs);
        } else {
            if (directory != utils::fs::path("."))
                test_programs.push_back(directory / line);
            else
                test_programs.push_back(utils::fs::path(line));
        }
    }
}


}  // anonymous namespace


/// Constructs a kyuafile form initialized data.
///
/// Use load() to parse a test suite configuration file and construct a
/// kyuafile object.
///
/// \param tps_ Collection of test program executables that belong to this test
///     suite.
engine::kyuafile::kyuafile(const std::vector< utils::fs::path >& tps_) :
    _test_programs(tps_)
{
}


/// Parses a test suite configuration file.
///
/// \param config_file The file to parse.
///
/// \return High-level representation of the configuration file.
///
/// \throw error If the file does not exist.  TODO(jmmv): This exception is not
///     accurate enough.
engine::kyuafile
engine::kyuafile::load(const utils::fs::path& config_file)
{
    std::vector< utils::fs::path > test_programs;
    load_one(config_file, config_file.branch_path(), test_programs);
    return kyuafile(test_programs);
}


/// Constructs a test suite based on command line arguments.
///
/// TODO(jmmv): This probably belongs in cli/.
///
/// \param args The command line arguments.
///
/// \return An adhoc test suite configuration based on the arguments.
///
/// \throw cmdline::usage_error If the arguments are invalid.
engine::kyuafile
engine::kyuafile::from_arguments(const cmdline::args_vector& args)
{
    std::vector< utils::fs::path > test_programs;
    for (cmdline::args_vector::const_iterator iter = args.begin();
         iter != args.end(); iter++) {
        if ((*iter).find(":") != std::string::npos) {
            throw cmdline::usage_error(F("Specifying a single test case to run "
                                         "is not implemented yet (arg %s)") %
                                       *iter);
        }

        // TODO(jmmv): Scan directories, if specified.

        try {
            test_programs.push_back(fs::path(*iter));
        } catch (const fs::invalid_path_error& error) {
            throw cmdline::usage_error(F("Invalid path '%s'") % *iter);
        }
    }
    return kyuafile(test_programs);
}


/// Gets the collection of test programs that belong to this test suite.
///
/// \return Collection of test program executable names.
const std::vector< utils::fs::path >&
engine::kyuafile::test_programs(void) const
{
    return _test_programs;
}
