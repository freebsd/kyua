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

#include "cli/cmd_test.hpp"
#include "cli/common.ipp"
#include "engine/drivers/run_tests.hpp"
#include "engine/test_case.hpp"
#include "engine/test_result.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui.hpp"
#include "utils/format/macros.hpp"

namespace cmdline = utils::cmdline;
namespace fs = utils::fs;
namespace run_tests = engine::drivers::run_tests;
namespace user_files = engine::user_files;

using cli::cmd_test;


namespace {


class hooks : public run_tests::base_hooks {
    cmdline::ui* _ui;

public:
    unsigned long good_count;
    unsigned long bad_count;

    hooks(cmdline::ui* ui_) :
        _ui(ui_),
        good_count(0),
        bad_count(0)
    {
    }

    virtual void
    got_result(const engine::test_case_id& id,
               const engine::test_result& result)
    {
        _ui->out(F("%s  ->  %s") % id.str() % cli::format_result(result));
        if (result.good())
            good_count++;
        else
            bad_count++;
    }
};


}  // anonymous namespace


/// Default constructor for cmd_test.
cmd_test::cmd_test(void) : cli_command(
    "test", "[test-program ...]", 0, -1, "Run tests")
{
    add_option(kyuafile_option);
    add_option(store_option);
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
    hooks h(ui);
    const run_tests::result result = run_tests::drive(
        kyuafile_path(cmdline), store_path(cmdline),
        parse_filters(cmdline.arguments()), config, h);

    int exit_code;
    if (h.good_count > 0 || h.bad_count > 0) {
        ui->out("");
        ui->out(F("%d/%d passed (%d failed)") % h.good_count %
                (h.good_count + h.bad_count) % h.bad_count);

        exit_code = (h.bad_count == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    } else
        exit_code = EXIT_SUCCESS;

    ui->out(F("Committed action %d") % result.action_id);

    return report_unused_filters(result.unused_filters, ui) ?
        EXIT_FAILURE : exit_code;
}
