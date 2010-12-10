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

#include <cstdlib>
#include <utility>
#include <vector>

#include "cli/cmd_list.hpp"
#include "engine/suite_config.hpp"
#include "engine/test_case.hpp"
#include "engine/test_program.hpp"
#include "utils/cmdline/base_command.ipp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/sanity.hpp"

namespace cmdline = utils::cmdline;

using cli::cmd_list;


/// Default constructor for cmd_list.
cmd_list::cmd_list(void) : cmdline::base_command(
    "list", "[test-program ...]", 0, -1,
    "Lists test cases and their meta-data")
{
    add_option(cmdline::path_option(
        'c', "suite_config", "Configuration file", "file", "kyua.suite"));
    add_option(cmdline::bool_option('v', "verbose", "Show properties"));
}


/// Entry point for the "list" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
///
/// \return 0 to indicate success.
int
cmd_list::run(cmdline::ui* ui, const cmdline::parsed_cmdline& cmdline)
{
    std::vector< utils::fs::path > test_programs;
    if (cmdline.arguments().empty()) {
        engine::suite_config suite_config = engine::suite_config::load(
            cmdline.get_option< cmdline::path_option >("suite_config"));
        test_programs = suite_config.test_programs();
    } else {
        engine::suite_config suite_config = engine::suite_config::from_arguments(
            cmdline.arguments());
        test_programs = suite_config.test_programs();
    }

    for (std::vector< utils::fs::path >::const_iterator p = test_programs.begin();
         p != test_programs.end(); p++) {
        const engine::test_cases_vector tcs = engine::load_test_cases(*p);

        for (engine::test_cases_vector::const_iterator iter = tcs.begin();
             iter != tcs.end(); iter++) {
            const engine::test_case& tc = *iter;
            ui->out(tc.identifier.str());

            if (cmdline.has_option("verbose")) {
                // TODO(jmmv): Print other metadata.
                for (engine::properties_map::const_iterator iter2 = tc.user_metadata.begin();
                     iter2 != tc.user_metadata.end(); iter2++)
                    ui->out(F("    %s = %s\n") % (*iter2).first % (*iter2).second);
            }
        }
    }

    return EXIT_SUCCESS;
}
