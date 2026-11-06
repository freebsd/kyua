// Copyright (c) 2024 Igor Ostapenko <igoro@FreeBSD.org>
// All rights reserved.
//
// SPDX-License-Identifier: BSD-2-Clause

#include "engine/prepare/prepare_all.hpp"

static const std::string _name = "all";
static const std::string _description = "Run all preparations";

namespace prepare = engine::prepare;


const std::string&
prepare::prepare_all::name() const
{
    return _name;
}


const std::string&
prepare::prepare_all::description() const
{
    return _description;
}


int
prepare::prepare_all::exec(cmdline::ui* ui,
                           const cmdline::parsed_cmdline& cmdline,
                           const config::tree& user_config) const
{
    for (auto& handler : prepare::handlers()) {
        if (handler->name() == this->name())
            continue;

        int error = handler->exec(ui, cmdline, user_config);
        if (error != EXIT_SUCCESS)
            return error;
    }

    return EXIT_SUCCESS;
}
