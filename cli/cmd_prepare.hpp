// Copyright (c) 2024 Igor Ostapenko <igoro@FreeBSD.org>
// All rights reserved.
//
// SPDX-License-Identifier: BSD-2-Clause

/// \file cli/cmd_prepare.hpp
/// Provides the cmd_prepare class.

#if !defined(CLI_CMD_PREPARE_HPP)
#define CLI_CMD_PREPARE_HPP

#include "cli/common.hpp"

namespace cli {


/// Implementation of the "prepare" subcommand.
class cmd_prepare : public cli_command
{
public:
    cmd_prepare(void);

    int run(utils::cmdline::ui*, const utils::cmdline::parsed_cmdline&,
            const utils::config::tree&);
};


}  // namespace cli

#endif  // !defined(CLI_CMD_PREPARE_HPP)
