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
#include "cli/filters.hpp"
#include "engine/user_files/kyuafile.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"

namespace cmdline = utils::cmdline;
namespace fs = utils::fs;
namespace user_files = engine::user_files;


/// Standard definition of the option to specify a Kyuafile.
///
/// You must use load_kyuafile() to load a configuration file while honoring the
/// value of this flag.
const cmdline::path_option cli::kyuafile_option(
    'k', "kyuafile",
    "Path to the test suite definition",
    "file", "Kyuafile");


/// Loads the Kyuafile for this session or generates a fake one.
///
/// The algorithm implemented here is as follows:
/// 1) If there are arguments on the command line that are supposed to override
///    the Kyuafile, the Kyuafile is not loaded and a fake one is generated.
/// 2) Otherwise, the user-provided Kyuafile is loaded.
///
/// \param cmdline The parsed command line.
///
/// \throw engine::error If the parsing of the configuration file fails.
///     TODO(jmmv): I'm not sure if this is the raised exception.  And even if
///     it is, we should make it more accurate.
user_files::kyuafile
cli::load_kyuafile(const cmdline::parsed_cmdline& cmdline)
{
    const fs::path filename = cmdline.get_option< cmdline::path_option >(
        kyuafile_option.long_name());

    return user_files::kyuafile::load(filename);
}


/// Internal implementation for cli::filters_state.
struct cli::filters_state::impl {
    /// The collection of filters provided by the user.
    test_filters filters;

    /// The filters that have been used so far.
    std::set< test_filter > used_filters;

    /// Constructs the internal representation of the filters.
    ///
    /// \param filters_ The filters provided by the user, already sanitized.
    impl(const std::set< test_filter >& filters_) :
        filters(filters_)
    {
    }
};


/// Parses a set of command-line arguments to construct test filters.
///
/// \param args The command-line arguments representing test filters.
///
/// \throw cmdline:error If any of the arguments is invalid, or if they
///     represent a non-disjoint collection of filters.
cli::filters_state::filters_state(const cmdline::args_vector& args)
{
    std::set< test_filter > filters;

    try {
        for (cmdline::args_vector::const_iterator iter = args.begin();
             iter != args.end(); iter++) {
            const test_filter filter(test_filter::parse(*iter));
            if (filters.find(filter) != filters.end())
                throw cmdline::error(F("Duplicate filter '%s'") % filter.str());
            filters.insert(filter);
        }
        check_disjoint_filters(filters);
    } catch (const std::runtime_error& e) {
        throw cmdline::error(e.what());
    }

    _pimpl.reset(new impl(filters));
}


/// Destructor.
///
/// This is needed to ensures that the pimpl object gets deleted by giving
/// visibility of the impl type to the smart poiner.
cli::filters_state::~filters_state(void)
{
}


/// Checks whether these filters match the given test program.
///
/// \param test_program The test program to match against.
///
/// \return True if these filters match the given test program name.
bool
cli::filters_state::match_test_program(const fs::path& test_program) const
{
    return _pimpl->filters.match_test_program(test_program);
}


/// Checks whether these filters match the given test case.
///
/// \param test_case The test case to match against.
///
/// \return True if these filters match the given test case identifier.
bool
cli::filters_state::match_test_case(const engine::test_case_id& test_case) const
{
    test_filters::match match = _pimpl->filters.match_test_case(test_case);
    if (match.first && match.second)
        _pimpl->used_filters.insert(match.second.get());
    return match.first;
}


/// Reports the filters that have not matched any tests as errors.
///
/// \param ui The user interface object through which errors are to be reported.
///
/// \return True if there are any unused filters.  The caller should report this
/// as an error to the user by means of a non-successful exit code.
bool
cli::filters_state::report_unused_filters(cmdline::ui* ui) const
{
    const std::set< test_filter > unused = _pimpl->filters.difference(
        _pimpl->used_filters);

    for (std::set< test_filter >::const_iterator iter = unused.begin();
         iter != unused.end(); iter++) {
        cmdline::print_warning(ui, F("No test cases matched by the filter '%s'")
                               % (*iter).str());
    }

    return !unused.empty();
}
