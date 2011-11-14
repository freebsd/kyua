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

#include "cli/cmd_report.hpp"
#include "cli/common.ipp"
#include "engine/action.hpp"
#include "engine/context.hpp"
#include "engine/drivers/scan_action.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/optional.ipp"

namespace cmdline = utils::cmdline;
namespace scan_action = engine::drivers::scan_action;
namespace user_files = engine::user_files;

using cli::cmd_report;
using utils::optional;


namespace {


/// Generates a plain-text report intended to be printed to the console.
class console_hooks : public scan_action::base_hooks {
    /// The user interface to which to send the output.
    cmdline::ui* const _ui;

    /// Whether to include the runtime context in the output or not.
    const bool _include_context;

    /// Dumps the execution context to the output.
    ///
    /// \param context The context to dump.
    void
    print_context(const engine::context& context) const
    {
        _ui->out("===> Execution context");

        _ui->out(F("Current directory: %s") % context.cwd());
        const std::map< std::string, std::string >& env = context.env();
        if (env.empty())
            _ui->out("No environment variables recorded");
        else {
            _ui->out("Environment variables:");
            for (std::map< std::string, std::string >::const_iterator
                     iter = env.begin(); iter != env.end(); iter++) {
                _ui->out(F("    %s=%s") % (*iter).first % (*iter).second);
            }
        }
    }

public:
    /// Constructor for the hooks.
    ///
    /// \param ui_ The user interface to which to send the output.
    /// \param include_context_ Whether to include the runtime context in
    ///     the output or not.
    console_hooks(cmdline::ui* ui_, bool include_context_) :
        _ui(ui_),
        _include_context(include_context_)
    {
    }

    /// Callback executed when an action is found.
    ///
    /// \param action The action loaded from the database.
    void
    got_action(const engine::action& action)
    {
        if (_include_context)
            print_context(action.runtime_context());
    }
};


}  // anonymous namespace


/// Default constructor for cmd_report.
cmd_report::cmd_report(void) : cli_command(
    "report", "", 0, 0,
    "Generates a report with the result of a previous action")
{
    add_option(store_option);
    add_option(cmdline::bool_option(
        "hide-context", "Do not include the execution context in the report"));
    add_option(cmdline::int_option(
        "action", "The action to report; if not specified, defaults to the "
        "latest action in the database", "id"));
    add_option(cmdline::string_option(
        'o', "output", "The format of the output and the location for the "
        "output", "format:output", "console:/dev/stdout"));
}


/// Entry point for the "report" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
/// \param unused_config The runtime configuration of the program.
///
/// \return 0 if everything is OK, 1 if the statement is invalid or if there is
/// any other problem.
int
cmd_report::run(cmdline::ui* ui, const cmdline::parsed_cmdline& cmdline,
                const user_files::config& UTILS_UNUSED_PARAM(config))
{
    if (cmdline.get_option< cmdline::string_option >("output") !=
        "console:/dev/stdout") {
        throw cmdline::usage_error("Support to change --output not yet "
                                   "implemented");
    }

    optional< int64_t > action_id;
    if (cmdline.has_option("action"))
        action_id = cmdline.get_option< cmdline::int_option >("action");

    console_hooks hooks(ui, !cmdline.has_option("hide-context"));
    scan_action::drive(store_path(cmdline), action_id, hooks);

    return EXIT_SUCCESS;
}
