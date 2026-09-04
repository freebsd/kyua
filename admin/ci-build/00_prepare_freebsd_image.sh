#!/bin/sh
#
# Step 0: Prepare the FreeBSD VM image for use.
#
# SPDX-License-Identifier: BSD-2-Clause

set -eux

VERSION_MAJOR="$(uname -r | sed -Ee 's/\..+//')"
if [ "${VERSION_MAJOR}" != 14 ]; then
    exit 0
fi

# NO_CHECK_STYLE_BEGIN
pkgbasify_sha256="7f3a4d514c46d03ad52d6689b621f4971ce85db42b1a41cabb18913e593357ad"
pkgbasify_url="https://github.com/FreeBSDFoundation/pkgbasify/raw/refs/tags/v1.1.0/pkgbasify.lua"
# NO_CHECK_STYLE_END
pkgbasify_script="pkgbasify.lua"
fetch -o "${pkgbasify_script}" "${pkgbasify_url}"
chmod +x "${pkgbasify_script}"
sha256 -c "${pkgbasify_sha256}" "${pkgbasify_script}"
pkg update
pkg upgrade -y pkg
yes y | "./${pkgbasify_script}" --force

# vim: syntax=sh:expandtab:shiftwidth=4:softtabstop=4
