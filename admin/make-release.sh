#!/bin/sh
#
# Create release artifacts from a release tag.
#
# Example:
# 	./admin/make-release.sh kyua-0.13

set -eux

tag=$1

cd "$(dirname "$(dirname "$0")")"

mkdir -p releases
release_root=$(realpath releases)

release_dir="${release_root}/${tag}"
release_artifact="${release_root}/${tag}.tar.gz"

rm -Rf "${release_dir}"
mkdir -p "${release_dir}"
git archive "${tag}" | tar xzvf - -C "${release_dir}"
cd "${release_dir}"
autoreconf -isv
./configure --enable-atf
make dist
mv *.tar.gz "${release_root}/${tag}.tar.gz"
cd "${release_root}"
sha256 "${release_artifact##*/}" > "${release_artifact}.sha256"

# vim: syntax=sh
