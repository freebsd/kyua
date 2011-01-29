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

#include "engine/exceptions.hpp"
#include "engine/user_files/common.hpp"
#include "engine/user_files/kyuafile.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/lua/exceptions.hpp"
#include "utils/lua/operations.hpp"
#include "utils/lua/wrap.ipp"

namespace cmdline = utils::cmdline;
namespace fs = utils::fs;
namespace lua = utils::lua;
namespace user_files = engine::user_files;


namespace {


/// Processes a raw list of test programs for use within this module.
///
/// \param strings_vector The input vector of strings.
/// \param root The directory where the initial Kyuafile is located.
///
/// \return A list of paths (not strings) in which any relative path has been
/// adjusted with the provided root directory.
///
/// throw fs::error If there is any invalid path in the input vector.
std::vector< fs::path >
adjust_test_programs(const std::vector< std::string >& strings,
                     const fs::path& root)
{
    std::vector< fs::path > paths;

    for (std::vector< std::string >::const_iterator iter = strings.begin();
         iter != strings.end(); iter++) {
        const fs::path raw_path(*iter);

        if (raw_path.is_absolute())
            paths.push_back(raw_path);
        else {
            if (root.str() == ".")
                paths.push_back(raw_path);
            else
                paths.push_back(root / raw_path);
        }
    }

    return paths;
}


}  // anonymous namespace


/// Constructs a kyuafile form initialized data.
///
/// Use load() to parse a test suite configuration file and construct a
/// kyuafile object.
///
/// \param tps_ Collection of test program executables that belong to this test
///     suite.
user_files::kyuafile::kyuafile(const std::vector< utils::fs::path >& tps_) :
    _test_programs(tps_)
{
}


/// Parses a test suite configuration file.
///
/// \param file The file to parse.
///
/// \return High-level representation of the configuration file.
///
/// \throw error If the file does not exist.  TODO(jmmv): This exception is not
///     accurate enough.
user_files::kyuafile
user_files::kyuafile::load(const utils::fs::path& file)
{
    std::vector< std::string > test_programs;
    try {
        lua::state state;
        lua::stack_cleaner cleaner(state);

        user_files::do_user_file(state, file);

        test_programs = lua::get_array_as_strings(state,
                                                  "kyuafile.TEST_PROGRAMS");
    } catch (const lua::error& e) {
        throw engine::error(F("Load failed: %s") % e.what());
    }
    try {
        return kyuafile(adjust_test_programs(test_programs,
                                             file.branch_path()));
    } catch (const fs::error& e) {
        throw engine::error(F("Load failed: %s") % e.what());
    }
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
user_files::kyuafile
user_files::kyuafile::from_arguments(const cmdline::args_vector& args)
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
user_files::kyuafile::test_programs(void) const
{
    return _test_programs;
}
