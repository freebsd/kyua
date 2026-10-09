// Copyright (c) 2024 Igor Ostapenko <igoro@FreeBSD.org>
// All rights reserved.
//
// SPDX-License-Identifier: BSD-2-Clause

/// \file engine/prepare/prepare_all.hpp
/// The requirement preparation handler which runs all registered handlers.

#if !defined(ENGINE_PREPARE_ALL_HPP)
#define ENGINE_PREPARE_ALL_HPP

#include "engine/prepare/prepare.hpp"

namespace engine {
namespace prepare {


class prepare_all : public prepare::handler {
public:
    const std::string& name() const;
    const std::string& description() const;
    int exec(cmdline::ui*, const cmdline::parsed_cmdline&,
             const config::tree&) const;
};


}  // namespace prepare
}  // namespace engine

#endif  // !defined(ENGINE_PREPARE_ALL_HPP)
