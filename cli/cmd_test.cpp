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

#include "cli/cmd_test.hpp"
#include "engine/results.hpp"
#include "engine/runner.hpp"
#include "engine/suite_config.hpp"
#include "utils/cmdline/base_command.ipp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui.hpp"
#include "utils/format/macros.hpp"


namespace cmdline = utils::cmdline;
namespace results = engine::results;
namespace runner = engine::runner;

using cli::cmd_test;


namespace {


class run_hooks : public runner::hooks {
    cmdline::ui* _ui;

public:
    unsigned long good_count;
    unsigned long bad_count;

    run_hooks(cmdline::ui* ui_) :
        _ui(ui_),
        good_count(0),
        bad_count(0)
    {
    }

    void
    start_test_case(const engine::test_case_id& identifier)
    {
    }

    void
    finish_test_case(const engine::test_case_id& identifier,
                     std::auto_ptr< const results::base_result > result)
    {
        _ui->out(F("%s  ->  %s") % identifier.str() % result->format());

        if (result->good())
            good_count++;
        else
            bad_count++;
    }
};


}  // anonymous namespace


/// Default constructor for cmd_test.
cmd_test::cmd_test(void) : cmdline::base_command(
    "test", "[test-program ...]", 0, -1,
    "Run tests")
{
    add_option(cmdline::path_option(
        'c', "suite_config", "Configuration file", "file", "kyua.suite"));
}


/// Entry point for the "test" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
///
/// \return 0 if all tests passed, 1 otherwise.
int
cmd_test::run(cmdline::ui* ui, const cmdline::parsed_cmdline& cmdline)
{
    run_hooks hooks(ui);

    if (cmdline.arguments().empty()) {
        const engine::suite_config suite = engine::suite_config::load(
            cmdline.get_option< cmdline::path_option >("suite_config"));
        runner::run_test_suite(suite, engine::properties_map(), &hooks);
    } else {
        const engine::suite_config suite = engine::suite_config::from_arguments(
            cmdline.arguments());
        runner::run_test_suite(suite, engine::properties_map(), &hooks);
    }

    ui->out("");
    ui->out(F("%d/%d passed (%d failed)") % hooks.good_count %
            (hooks.good_count + hooks.bad_count) % hooks.bad_count);

    return hooks.bad_count == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
