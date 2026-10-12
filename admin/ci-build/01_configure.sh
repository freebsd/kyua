#!/bin/sh
#
# Copyright (c) 2026 Enji Cooper.
#
# SPDX-License-Identifier: BSD-2-Clause

# Step 1: run autoconf/configure so the remaining of the build/test steps can
# be completed.
#
# Splitting off this step allows any configure issues to be found quickly and
# triaged more effectively.

autoreconf_args="-isv"
: "${PREFIX=/usr/local}"
if [ -d ${PREFIX}/share/aclocal ]; then
    autoreconf_args="${autoreconf_args} -I${PREFIX}/share/aclocal"
fi
# shellcheck disable=SC2086
autoreconf ${autoreconf_args}

# shellcheck disable=SC2086
if ! ./configure $*; then
    cat config.log || true
    exit 1
fi

# vim: syntax=sh:expandtab:shiftwidth=4:softtabstop=4
