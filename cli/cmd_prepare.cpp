// Copyright (c) 2024 Igor Ostapenko <igoro@FreeBSD.org>
// All rights reserved.
//
// SPDX-License-Identifier: BSD-2-Clause

#include "cli/cmd_prepare.hpp"

#include "cli/common.ipp"
#include "engine/prepare/prepare.hpp"
#include "utils/cmdline/options.hpp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;
namespace prepare = engine::prepare;

using cli::cmd_prepare;


/// Default constructor for cmd_prepare.
cmd_prepare::cmd_prepare(void) : cli_command(
    "prepare", "[handler-name ...]", 0, -1,
    "Prepares environment and declared requirements before testing")
{
    add_option(kyuafile_option);
    add_option(build_root_option);
    add_option(cmdline::bool_option('n', "dry-run", "Do not alter the system"));
}


/// Entry point for the "prepare" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
/// \param user_config The runtime configuration of the program.
///
/// \return 0 if successful, 1 otherwise.
int
cmd_prepare::run(cmdline::ui* ui, const cmdline::parsed_cmdline& cmdline,
            const config::tree& user_config)
{
    // List available preparation handlers
    if (cmdline.arguments().empty()) {
        for (auto& h : prepare::handlers()) {
            ui->out(h->name(), false);
            ui->out("\t\t\t", false);
            ui->out(h->description());
        }
        return EXIT_SUCCESS;
    }

    // Or run the given ones
    return prepare::run(cmdline.arguments(), ui, cmdline, user_config);
}
