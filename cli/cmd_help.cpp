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

#include "cli/all_commands.hpp"
#include "cli/cmd_help.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/globals.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/cmdline/ui.hpp"
#include "utils/format/macros.hpp"
#include "utils/sanity.hpp"

namespace cmdline = utils::cmdline;

using cli::cmd_help;


namespace {


/// Prints the summary of commands and generic options.
///
/// \param ui Object to interact with the I/O of the program.
static void
general_help(cmdline::ui* ui)
{
    ui->out(F("Usage: %s [general_options] command [options] [args]") %
              cmdline::progname());

    ui->out("");
    ui->out("Available commands:");
    for (cmdline::base_command** iter = cli::all_commands; *iter != NULL;
         iter++) {
        ui->out(F("    %s: %s.") % (*iter)->name() %
                (*iter)->short_description());
    }
}


/// Prints help for a set of options.
///
/// \param ui Object to interact with the I/O of the program.
/// \param options The set of options to describe.
static void
options_help(cmdline::ui* ui, const cmdline::options_vector& options)
{
    PRE(!options.empty());

    ui->out("Available options:");
    for (cmdline::options_vector::const_iterator iter = options.begin();
         iter != options.end(); iter++) {
        const cmdline::base_option* option = *iter;

        std::string description = option->description();
        if (option->needs_arg() && option->has_default_value())
            description += F(" (default: %s)") % option->default_value();

        if (option->has_short_name())
            ui->out(F("    %s, %s: %s.") % option->format_short_name() %
                    option->format_long_name() % description);
        else
            ui->out(F("    %s: %s.") % option->format_long_name() %
                    description);
    }
}


/// Prints help for a particular subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param command Pointer to the command to describe.
static void
subcommand_help(cmdline::ui* ui, const utils::cmdline::base_command* command)
{
    ui->out(F("Usage: %s [general_options] %s%s%s") %
            cmdline::progname() % command->name() %
            (command->options().empty() ? "" : " [options]") %
            (command->arg_list().empty() ? "" : (" " + command->arg_list())));
    ui->out("");
    ui->out(F("%s.") % command->short_description());

    const cmdline::options_vector& options = command->options();
    if (!options.empty()) {
        ui->out("");
        options_help(ui, options);
    }
}


}  // anonymous namespace


/// Default constructor for cmd_help.
cmd_help::cmd_help(void) : cmdline::base_command(
    "help", "[subcommand]", 0, 1,
    "Shows usage information")
{
}


/// Entry point for the "help" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
///
/// \return 0 to indicate success.
int
cmd_help::run(utils::cmdline::ui* ui,
              const utils::cmdline::parsed_cmdline& cmdline)
{
    if (cmdline.arguments().empty()) {
        general_help(ui);
    } else {
        INV(cmdline.arguments().size() == 1);
        const std::string& cmdname = cmdline.arguments()[0];
        const cmdline::base_command* command = cli::find_command(cmdname);
        if (command == NULL)
            throw cmdline::usage_error(F("The command %s does not exist") %
                                       cmdname);
        subcommand_help(ui, command);
    }

    return EXIT_SUCCESS;
}
