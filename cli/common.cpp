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

#include <algorithm>

#include "cli/common.hpp"
#include "engine/filters.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"

namespace cmdline = utils::cmdline;
namespace fs = utils::fs;
namespace user_files = engine::user_files;


/// Standard definition of the option to specify a Kyuafile.
const cmdline::path_option cli::kyuafile_option(
    'k', "kyuafile",
    "Path to the test suite definition",
    "file", "Kyuafile");


/// Gets the path to the Kyuafile to be loaded.
///
/// This is just syntactic sugar to simplify quierying the 'kyuafile_option'.
///
/// \param cmdline The parsed command line.
///
/// \return The path to the Kyuafile to be loaded.
fs::path
cli::kyuafile_path(const cmdline::parsed_cmdline& cmdline)
{
    return cmdline.get_option< cmdline::path_option >(
        kyuafile_option.long_name());
}


/// Parses a set of command-line arguments to construct test filters.
///
/// \param args The command-line arguments representing test filters.
///
/// \throw cmdline:error If any of the arguments is invalid, or if they
///     represent a non-disjoint collection of filters.
std::set< engine::test_filter >
cli::parse_filters(const cmdline::args_vector& args)
{
    std::set< engine::test_filter > filters;

    try {
        for (cmdline::args_vector::const_iterator iter = args.begin();
             iter != args.end(); iter++) {
            const engine::test_filter filter(engine::test_filter::parse(*iter));
            if (filters.find(filter) != filters.end())
                throw cmdline::error(F("Duplicate filter '%s'") % filter.str());
            filters.insert(filter);
        }
        check_disjoint_filters(filters);
    } catch (const std::runtime_error& e) {
        throw cmdline::error(e.what());
    }

    return filters;
}


/// Reports the filters that have not matched any tests as errors.
///
/// \param ui The user interface object through which errors are to be reported.
///
/// \return True if there are any unused filters.  The caller should report this
/// as an error to the user by means of a non-successful exit code.
bool
cli::report_unused_filters(const std::set< engine::test_filter >& unused,
                           cmdline::ui* ui)
{
    for (std::set< engine::test_filter >::const_iterator iter = unused.begin();
         iter != unused.end(); iter++) {
        cmdline::print_warning(ui, F("No test cases matched by the filter '%s'")
                               % (*iter).str());
    }

    return !unused.empty();
}
