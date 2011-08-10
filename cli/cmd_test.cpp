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

#include <cstdlib>
#include <utility>

#include "cli/cmd_test.hpp"
#include "cli/common.ipp"
#include "engine/results.hpp"
#include "engine/test_program.hpp"
#include "engine/user_files/config.hpp"
#include "engine/user_files/kyuafile.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui.hpp"
#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"


namespace cmdline = utils::cmdline;
namespace fs = utils::fs;
namespace results = engine::results;
namespace user_files = engine::user_files;

using cli::cmd_test;


/// Runs a test program in a controlled manner.
///
/// If the test program fails to provide a list of test cases, a fake test case
/// named '__test_program__' is created and it is reported as broken.
///
/// \param test_program The test program to execute.
/// \param config The configuration variables provided by the user.
/// \param ui Interface for progress reporting.
static std::pair< unsigned long, unsigned long >
run_test_program(const engine::base_test_program& test_program,
                 const user_files::config& config,
                 const cli::filters_state& filters,
                 cmdline::ui* ui)
{
    LI(F("Processing test program '%s'") % test_program.relative_path());

    engine::test_cases_vector test_cases;
    try {
        test_cases = test_program.test_cases();
    } catch (const std::exception& e) {
        const results::broken broken(F("Failed to load list of test cases: "
                                       "%s") % e.what());
        // TODO(jmmv): Maybe generalize this in test_case_id somehow?
        const engine::test_case_id program_id(
            test_program.relative_path(), "__test_program__");
        ui->out(F("%s  ->  %s") % program_id.str() %
                broken.format());
        return std::make_pair(0, 1);
    }

    unsigned long good_count = 0;
    unsigned long bad_count = 0;
    for (engine::test_cases_vector::const_iterator iter = test_cases.begin();
         iter != test_cases.end(); iter++) {
        const engine::test_case_ptr test_case = *iter;

        if (!filters.match_test_case(test_case->identifier()))
            continue;

        results::result_ptr result = test_case->run(config);

        ui->out(F("%s  ->  %s") % test_case->identifier().str() %
                result->format());
        if (result->good())
            good_count++;
        else
            bad_count++;
    }
    return std::make_pair(good_count, bad_count);
}


/// Default constructor for cmd_test.
cmd_test::cmd_test(void) : cli_command(
    "test", "[test-program ...]", 0, -1, "Run tests")
{
    add_option(kyuafile_option);
}


/// Entry point for the "test" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
/// \param config The runtime configuration of the program.
///
/// \return 0 if all tests passed, 1 otherwise.
int
cmd_test::run(cmdline::ui* ui, const cmdline::parsed_cmdline& cmdline,
              const user_files::config& config)
{
    const cli::filters_state filters(cmdline.arguments());
    const user_files::kyuafile kyuafile = load_kyuafile(cmdline);

    unsigned long good_count = 0;
    unsigned long bad_count = 0;
    for (engine::test_programs_vector::const_iterator p =
         kyuafile.test_programs().begin(); p != kyuafile.test_programs().end();
         p++) {
        const engine::test_program_ptr& test_program = *p;

        if (!filters.match_test_program(test_program->relative_path()))
            continue;

        const std::pair< unsigned long, unsigned long > partial =
            run_test_program(*test_program, config, filters, ui);
        good_count += partial.first;
        bad_count += partial.second;
    }

    int exit_code;
    if (good_count > 0 || bad_count > 0) {
        ui->out("");
        ui->out(F("%d/%d passed (%d failed)") % good_count %
                (good_count + bad_count) % bad_count);

        exit_code = (bad_count == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    } else
        exit_code = EXIT_SUCCESS;

    return filters.report_unused_filters(ui) ? EXIT_FAILURE : exit_code;
}
