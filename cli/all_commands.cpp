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

#include "cli/all_commands.hpp"
#include "cli/cmd_help.hpp"
#include "cli/cmd_list.hpp"
#include "cli/cmd_test.hpp"
#include "cli/cmd_version.hpp"
#include "utils/cmdline/base_command.hpp"
#include "utils/format/macros.hpp"
#include "utils/sanity.hpp"

namespace cmdline = utils::cmdline;


namespace {


static cli::cmd_help cmd_help_instance;
static cli::cmd_list cmd_list_instance;
static cli::cmd_test cmd_test_instance;
static cli::cmd_version cmd_version_instance;


static cmdline::base_command* commands_table[] = {
    &cmd_help_instance,
    &cmd_list_instance,
    &cmd_test_instance,
    &cmd_version_instance,
    NULL,
};


}  // anonymous namespace


cmdline::base_command** cli::all_commands = commands_table;


/// Locates a command by name.
///
/// \param name The name of the command; typically given by the user.
///
/// \return The command if the name is valid; NULL otherwise.
cmdline::base_command*
cli::find_command(const std::string& name)
{
    cmdline::base_command* command = NULL;
    for (cmdline::base_command** iter = all_commands;
         command == NULL && *iter != NULL; iter++) {
        if ((*iter)->name() == name)
            command = *iter;
    }
    return command;
}


/// Replaces the built-in commands with a different set.
///
/// This is provided solely for testing purposes.
///
/// \param commands NULL-terminated array of pointers describing the new
///     commands.
void
cli::set_commands_for_testing(utils::cmdline::base_command** commands)
{
    all_commands = commands;
}
