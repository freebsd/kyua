#!/bin/sh
#
# Copyright (c) 2026 Enji Cooper.
#
# SPDX-License-Identifier: BSD-2-Clause

# Step 2: run `make distcheck`.
#
# `make distcheck` builds the tests, runs them, and subsequently performs a
# style check on the code.

set -eux

: "${AS_ROOT=no}"
: "${CC=cc}"
: "${CXX=c++}"
: "${EXTRA_DISTCHECK_CONFIGURE_ARGS=}"
: "${RUN_COREDUMP_TESTS:=false}"
: "${UNPRIVILEGED_USER:=no}"

NPROC=$(nproc 2>/dev/null || getconf NPROCESSORS_ONLN 2>/dev/null || echo 1)

f=
# Is this being run in a git clone, or with a release artifact? If the former,
# automatically enable developer mode.
if git rev-parse --is-inside-work-tree; then
    f="${f} --enable-developer"
fi
if [ -n "${EXTRA_DISTCHECK_CONFIGURE_ARGS:-}" ]; then
    f="${f} ${EXTRA_DISTCHECK_CONFIGURE_ARGS}"
fi

kyua_conf="$(realpath "$(mktemp kyuaconf-XXXXXXXX)")"
trap 'rm -f "${kyua_conf}"' EXIT INT TERM

sudo=

cat >"${kyua_conf}" <<EOF
syntax(2)

-- We do not know how many CPUs the test machine has.  However, parallelizing
-- the execution of our tests to _any_ degree speeds up the time it takes to
-- complete a test run because many of our tests are blocking.
parallelism = 4

test_suites.kyua.run_coredump_tests = "${RUN_COREDUMP_TESTS}"
EOF

sudo=
if [ "${AS_ROOT}" = yes ]; then
    sudo="sudo -EH"
fi
if [ "${UNPRIVILEGED_USER}" = "yes" ]; then
    echo "unprivileged_user = 'nobody'" >>"${kyua_conf}"
fi
${sudo} env PATH="${PATH}" make distcheck -j"${NPROC}" \
    DISTCHECK_CONFIGURE_FLAGS="${f}" \
    KYUA_CONFIG_FILE_FOR_CHECK="${kyua_conf}"

# vim: syntax=sh:expandtab:shiftwidth=4:softtabstop=4:textwidth=80
