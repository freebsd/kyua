#!/bin/sh
#
# Copyright (c) 2026 Enji Cooper.
#
# SPDX-License-Identifier: BSD-2-Clause

# Step 0.a: build and install ATF.

set -eux

: "${PREFIX=/usr/local}"

autoreconf_args="-isv"
if [ -d "${PREFIX}/share/aclocal" ]; then
    autoreconf_args="${autoreconf_args} -I${PREFIX}/share/aclocal"
fi
# shellcheck disable=SC2086
autoreconf ${autoreconf_args}

# Don't enable dev mode.
f="--disable-developer --prefix=${PREFIX} $@"
# shellcheck disable=SC2086
if ! ./configure ${f}; then
    cat config.log || true
    exit 1
fi

make all
make install
make clean

# vim: syntax=sh:expandtab:shiftwidth=4:softtabstop=4
