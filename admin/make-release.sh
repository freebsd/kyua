#!/bin/sh
#
# Create release artifacts from a release tag.
#
# Example:
# 	./admin/make-release.sh kyua-0.13

readonly REPO=freebsd/kyua

set -eux

if ! gh auth status >/dev/null; then
	echo "${0##*/}: ERROR: please install 'gh' and run 'gh auth login'."
	exit 1
fi

tag=$1

cd "$(dirname "$(dirname "$0")")"

mkdir -p releases
release_root=$(realpath releases)

release_dir="${release_root}/${tag}"
release_artifact="${release_root}/${tag}.tar.gz"
release_artifact_checksum_file="${release_artifact}.sha256"
release_assets="${release_artifact} ${release_artifact_checksum_file}"

rm -Rf "${release_dir}"
mkdir -p "${release_dir}"
git archive "${tag}" | tar xzvf - -C "${release_dir}"
cd "${release_dir}"
autoreconf -isv
./configure --enable-atf
make dist
mv -- *.tar.gz "${release_root}/${tag}.tar.gz"
cd "${release_root}"
sha256 "${release_artifact##*/}" > "${release_artifact_checksum_file}"

# shellcheck disable=SC2086
gh release upload --repo "${REPO}" "${tag}" ${release_assets}

# vim: syntax=sh
