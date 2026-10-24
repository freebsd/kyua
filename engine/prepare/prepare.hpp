// Copyright (c) 2024 Igor Ostapenko <igoro@FreeBSD.org>
// All rights reserved.
//
// SPDX-License-Identifier: BSD-2-Clause

/// \file engine/prepare/prepare.hpp
/// Requirement preparation subsystem interface.

#if !defined(ENGINE_PREPARE_HPP)
#define ENGINE_PREPARE_HPP

#include <string>
#include <vector>

#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui.hpp"
#include "utils/config/tree.ipp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;


namespace engine {
namespace prepare {


/// Abstract interface of a requirement preparation handler.
class handler {
public:
    /// Constructor.
    handler() {}

    /// Destructor.
    virtual ~handler() {}

    /// Returns name of the handler.
    virtual const std::string& name() const = 0;

    /// Returns short description of the handler.
    virtual const std::string& description() const = 0;

    /// Runs the requirement preparation handler.
    ///
    /// \param ui Object to interact with the I/O of the program.
    /// \param cmdline Representation of the command line to the subcommand.
    /// \param user_config The runtime configuration of the program.
    ///
    /// \return 0 to indicate success.
    virtual int exec(cmdline::ui* ui,
                     const cmdline::parsed_cmdline& cmdline,
                     const config::tree& user_config) const = 0;
};


/// Registers a requirement preparation handler.
///
/// \param handler A requirement preparation handler.
void register_handler(const std::shared_ptr< handler > handler);


/// Returns the list of registered requirement preparation handlers.
///
/// \return A vector of pointers to requirement preparation handlers.
const std::vector< std::shared_ptr< handler > > handlers();


/// Run named handlers.
///
/// \param handler_names Names of the handlers to run.
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
/// \param user_config The runtime configuration of the program.
///
/// \return 0 to indicate success.
int run(const std::vector< std::string >& handler_names,
        cmdline::ui* ui,
        const cmdline::parsed_cmdline& cmdline,
        const config::tree& user_config);


}  // namespace prepare
}  // namespace engine

#endif  // !defined(ENGINE_PREPARE_HPP)
