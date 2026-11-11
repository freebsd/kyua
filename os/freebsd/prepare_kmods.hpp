// Copyright (c) 2024 Igor Ostapenko <igoro@FreeBSD.org>
// All rights reserved.
//
// SPDX-License-Identifier: BSD-2-Clause

/// \file os/freebsd/prepare_kmods.hpp
/// The preparation handler which loads FreeBSD modules declared via
/// "required_kmods" metadata.

#if !defined(FREEBSD_PREPARE_KMODS_HPP)
#define FREEBSD_PREPARE_KMODS_HPP

#include "engine/prepare/prepare.hpp"

namespace prepare = engine::prepare;


namespace freebsd {


class prepare_kmods : public prepare::handler {
public:
    const std::string& name() const;
    const std::string& description() const;
    int exec(cmdline::ui*, const cmdline::parsed_cmdline&,
             const config::tree&) const;
};


}  // namespace freebsd

#endif  // !defined(FREEBSD_PREPARE_KMODS_HPP)
