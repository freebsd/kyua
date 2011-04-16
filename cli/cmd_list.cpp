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
#include <vector>

#include "cli/cmd_list.hpp"
#include "cli/common.hpp"
#include "engine/test_case.hpp"
#include "engine/test_program.hpp"
#include "engine/user_files/kyuafile.hpp"
#include "utils/cmdline/base_command.ipp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/sanity.hpp"

namespace cmdline = utils::cmdline;
namespace user_files = engine::user_files;


/// Default constructor for cmd_list.
cli::cmd_list::cmd_list(void) : cmdline::base_command(
    "list", "[test-program ...]", 0, -1,
    "Lists test cases and their meta-data")
{
    add_option(kyuafile_option);
    add_option(cmdline::bool_option('v', "verbose", "Show properties"));
}


/// Entry point for the "list" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
///
/// \return 0 to indicate success.
int
cli::cmd_list::run(cmdline::ui* ui, const cmdline::parsed_cmdline& cmdline)
{
    const cli::test_filters filters(cmdline.arguments());
    const user_files::kyuafile kyuafile = load_kyuafile(cmdline);

    bool matched = false;

    for (user_files::test_programs_vector::const_iterator p =
         kyuafile.test_programs().begin(); p != kyuafile.test_programs().end();
         p++) {
        if (!filters.match_test_program((*p).binary_path))
            continue;

        const engine::test_cases_vector tcs = engine::load_test_cases(
            (*p).binary_path);

        for (engine::test_cases_vector::const_iterator iter = tcs.begin();
             iter != tcs.end(); iter++) {
            const engine::test_case& tc = *iter;
            if (!filters.match_test_case(tc.identifier))
                continue;

            matched = true;
            if (!cmdline.has_option("verbose")) {
                ui->out(tc.identifier.str());
            } else {
                ui->out(F("%s (%s)") % tc.identifier.str() %
                        (*p).test_suite_name);

                const engine::properties_map props = tc.all_properties();
                for (engine::properties_map::const_iterator iter2 = props.begin();
                     iter2 != props.end(); iter2++)
                    ui->out(F("    %s = %s") % (*iter2).first % (*iter2).second);
            }
        }
    }

    if (!matched) {
        // TODO(jmmv): Does not print a nice error prefix; must fix.
        ui->err("No test cases matched by the filters provided.");
        return EXIT_FAILURE;
    } else
        return EXIT_SUCCESS;
}
