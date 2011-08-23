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

#include <cstdlib>
#include <stdexcept>

#include "cli/cmd_debug.hpp"
#include "cli/common.ipp"
#include "cli/filters.hpp"
#include "engine/results.hpp"
#include "engine/user_files/config.hpp"
#include "engine/user_files/kyuafile.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"

namespace cmdline = utils::cmdline;
namespace results = engine::results;
namespace user_files = engine::user_files;

using cli::cmd_debug;


namespace {


/// Looks for a single test case in the Kyuafile.
///
/// \param filter A filter to match the desired test case.
/// \param kyuafile The test suite in which to look for the test case.
///
/// \return A pointer to the test case if found.
///
/// \throw std::runtime_error If the provided filter matches more than one test
///     case or if the test case cannot be found.
const engine::test_case_ptr
find_test_case(const cli::test_filter& filter,
               const user_files::kyuafile& kyuafile)
{
    engine::test_case_ptr found;;

    for (engine::test_programs_vector::const_iterator p =
         kyuafile.test_programs().begin(); p != kyuafile.test_programs().end();
         p++) {
        const engine::test_program_ptr& test_program = *p;

        if (!filter.matches_test_program(test_program->relative_path()))
            continue;

        const engine::test_cases_vector test_cases = test_program->test_cases();

        for (engine::test_cases_vector::const_iterator
             iter = test_cases.begin(); iter != test_cases.end(); iter++) {
            const engine::test_case_ptr tc = *iter;

            if (filter.matches_test_case(tc->identifier())) {
                if (found.get() != NULL)
                    throw std::runtime_error(F("The filter '%s' matches more "
                                               "than one test case") %
                                             filter.str());
                found = tc;
            }
        }
    }

    if (found.get() == NULL)
        throw std::runtime_error(F("Unknown test case '%s'") % filter.str());

    return found;
}


}  // anonymous namespace


/// Default constructor for cmd_debug.
cmd_debug::cmd_debug(void) : cli_command(
    "debug", "test_case", 1, 1,
    "Executes a single test case providing facilities for debugging")
{
    add_option(kyuafile_option);

    add_option(cmdline::path_option(
        "stdout", "Where to direct the standard output of the test case",
        "path", "/dev/stdout"));

    add_option(cmdline::path_option(
        "stderr", "Where to direct the standard error of the test case",
        "path", "/dev/stderr"));
}


/// Entry point for the "debug" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
/// \param config The runtime debuguration of the program.
///
/// \return 0 if everything is OK, 1 if any of the necessary documents cannot be
/// opened.
int
cmd_debug::run(cmdline::ui* ui, const cmdline::parsed_cmdline& cmdline,
               const user_files::config& config)
{
    const std::string& test_case_name = cmdline.arguments()[0];
    if (test_case_name.find(':') == std::string::npos)
        throw cmdline::usage_error(F("'%s' is not a test case identifier "
                                     "(missing ':'?)") % test_case_name);
    const test_filter filter = test_filter::parse(test_case_name);

    const user_files::kyuafile kyuafile = load_kyuafile(cmdline);

    const engine::test_case_ptr test_case = find_test_case(filter, kyuafile);
    const results::result_ptr result = test_case->debug(
        config,
        cmdline.get_option< cmdline::path_option >("stdout"),
        cmdline.get_option< cmdline::path_option >("stderr"));

    ui->out(F("%s  ->  %s") % test_case->identifier().str() % result->format());

    return result->good() ? EXIT_SUCCESS : EXIT_FAILURE;
}
