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
#include <iostream>
#include <string>
#include <utility>

#include "cli/all_commands.hpp"
#include "cli/main.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/globals.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/cmdline/ui.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/sanity.hpp"

namespace cmdline = utils::cmdline;
namespace fs = utils::fs;


/// Executes the given subcommand with proper usage_error reporting.
///
/// \param ui Object to interact with the I/O of the program.
/// \param command The subcommand to execute.
/// \param args The part of the command line passed to the subcommand.  The
///     first item of this collection must match the command name.
///
/// \return The exit code of the command.  Typically 0 on success, some other
/// integer otherwise.
///
/// \throw cmdline::usage_error If the user input to the subcommand is invalid.
///     This error does not encode the command name within it, so this function
///     extends the message in the error to specify which subcommand was
///     affected.
/// \throw std::exception This propagates any uncaught exception.  Such
///     exceptions are bugs, but we let them propagate so that the runtime will
///     abort and dump core.
static int
run_subcommand(cmdline::ui* ui, cmdline::base_command* command,
               const cmdline::args_vector& args)
{
    try {
        PRE(command->name() == args[0]);
        return command->main(ui, args);
    } catch (const cmdline::usage_error& e) {
        throw std::pair< std::string, cmdline::usage_error >(
            command->name(), e);
    }
}


/// Exception-safe version of main.
///
/// This function provides the real meat of the entry point of the program.  It
/// is allowed to throw some known exceptions which are parsed by the caller.
/// Doing so keeps this function simpler and allow tests to actually validate
/// that the errors reported are accurate.
///
/// \return The exit code of the program.  Typically 0 on success, some other
/// integer otherwise, but this depends on the subcommand executed (if any).
///
/// \param ui Object to interact with the I/O of the program.
/// \param argc The number of arguments passed on the command line.
/// \param argv NULL-terminated array containing the command line arguments.
///
/// \throw cmdline::usage_error If the user ran the program with invalid
///     arguments.
/// \throw std::exception This propagates any uncaught exception.  Such
///     exceptions are bugs, but we let them propagate so that the runtime will
///     abort and dump core.
static int
safe_main(cmdline::ui* ui, int argc, const char* const argv[])
{
    cmdline::options_vector options;
    cmdline::parsed_cmdline cmdline = cmdline::parse(argc, argv, options);

    if (cmdline.arguments().empty())
        throw cmdline::usage_error("No command provided");
    const std::string command = cmdline.arguments()[0];

    for (cmdline::base_command** iter = cli::all_commands; *iter != NULL;
         iter++) {
        if ((*iter)->name() == command) {
            return run_subcommand(ui, *iter, cmdline.arguments());
        }
    }
    throw cmdline::usage_error(F("Unknown command '%s'") % command);
}


/// Testable entry point, with catch-all exception handlers.
///
/// This entry point does not perform any initialization of global state; it is
/// provided to allow unit-testing of the utility's entry point.
///
/// \param ui Object to interact with the I/O of the program.
/// \param argc The number of arguments passed on the command line.
/// \param argv NULL-terminated array containing the command line arguments.
///
/// \return 0 on success, some other integer on error.
///
/// \throw std::exception This propagates any uncaught exception.  Such
///     exceptions are bugs, but we let them propagate so that the runtime will
///     abort and dump core.
int
cli::main(cmdline::ui* ui, const int argc, const char* const* const argv)
{
    try {
        return safe_main(ui, argc, argv);
    } catch (const std::pair< std::string, cmdline::usage_error >& e) {
        ui->err(F("Usage error for command %s: %s.") % e.first %
                e.second.what());
        ui->err(F("Type '%s help %s' for usage information.") %
                cmdline::progname() % e.first);
        return EXIT_FAILURE;
    } catch (const cmdline::usage_error& e) {
        ui->err(F("Usage error: %s.") % e.what());
        ui->err(F("Type '%s help' for usage information.") %
                cmdline::progname());
        return EXIT_FAILURE;
    }
}


/// Delegate for ::main().
///
/// This function is supposed to be called directly from the top-level ::main()
/// function.  It takes care of initializing internal libraries and then calls
/// main(ui, argc, argv).
///
/// \pre This function can only be called once.
///
/// \throw std::exception This propagates any uncaught exception.  Such
///     exceptions are bugs, but we let them propagate so that the runtime will
///     abort and dump core.
int
cli::main(const int argc, const char* const* const argv)
{
    cmdline::init(argv[0]);
    cmdline::ui ui;
    return main(&ui, argc, argv);
}
