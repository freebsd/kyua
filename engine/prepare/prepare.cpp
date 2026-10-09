// Copyright (c) 2024 Igor Ostapenko <igoro@FreeBSD.org>
// All rights reserved.
//
// SPDX-License-Identifier: BSD-2-Clause

#include "engine/prepare/prepare.hpp"

#include "engine/prepare/prepare_all.hpp"

namespace prepare = engine::prepare;


/// List of registered requirement preparation handlers.
///
/// Use register_handler() to add an entry to this global list.
static std::vector< std::shared_ptr< prepare::handler > > _handlers = {
    std::shared_ptr< prepare::handler >(new prepare::prepare_all())
};


void
prepare::register_handler(const std::shared_ptr< handler > handler)
{
    _handlers.push_back(handler);
}


const std::vector< std::shared_ptr< prepare::handler > >
prepare::handlers()
{
    return _handlers;
}


int
prepare::run(const std::vector< std::string >& handler_names,
        cmdline::ui* ui,
        const cmdline::parsed_cmdline& cmdline,
        const config::tree& user_config)
{
    for (auto& hname : handler_names) {
        std::shared_ptr< prepare::handler > handler = nullptr;
        for (auto& h : prepare::handlers())
            if (h->name() == hname) {
                handler = h;
                break;
            }

        if (handler == nullptr) {
            ui->out(F("Unknown requirement preparation handler: %s") % hname);
            return EXIT_FAILURE;
        }

        if (handler->exec(ui, cmdline, user_config) != 0) {
            // suppress the actual code -- main limits possible exit codes
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
