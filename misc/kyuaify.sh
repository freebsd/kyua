#! /bin/sh
# Copyright 2011 Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
# * Neither the name of Google Inc. nor the names of its contributors
#   may be used to endorse or promote products derived from this software
#   without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# \file kyuaify.sh
# Converts Atffiles to Kyuafiles for a particular test suite.
#
# This script scans a test suite and converts all of its Atffiles into
# Kyuafiles.  The conversion is quite rudimentary but it works for, at least,
# the NetBSD test suite.
#
# Be aware that this wipes any existing Kyuafiles from the test suite directory
# before recreating the new ones.


set -e


# The program name (i.e. the name of the program without its directory).
ProgName="${0##*/}"


# Prints an informational message.
#
# \param ... The message to print.  Can be provided as multiple words and, in
#     that case, they are joined together by a single whitespace.
info() {
    echo "${ProgName}: I: $*" 1>&2
}


# Prints a runtime error and exits.
#
# \param ... The message to print.  Can be provided as multiple words and, in
#     that case, they are joined together by a single whitespace.
error() {
    echo "${ProgName}: E: $*" 1>&2
    exit 1
}


# Prints an usage error and exits.
#
# \param ... The message to print.  Can be provided as multiple words and, in
#     that case, they are joined together by a single whitespace.
usage_error() {
    echo "${ProgName}: E: $*" 1>&2
    echo "Usage: ${ProgName} <path-to-test-suite>" 1>&2
    exit 1
}


# Prunes all Kyuafiles from a test suite in preparation for regeneration.
#
# \param root The path to the test suite.
remove_kyuafiles() {
    local root="${1}"; shift

    info "Removing stale Kyuafiles from ${root}"
    find "${root}" -name Kyuafile -exec rm -f {} \;
}


# Obtains the list of test programs and subdirectories referenced by an Atffile.
#
# Any globs within the Atffile are expanded relative to the directory in which
# the Atffile lives.
#
# \param atffile The path to the Atffile to process.
#
# \post Prints the list of files referenced by the Atffile on stdout.
extract_files() {
    local atffile="${1}"; shift

    local dir="$(dirname "${atffile}")"

    local globs="$(grep '^tp-glob:' "${atffile}" | cut -d ' ' -f 2-)"
    local files="$(grep '^tp:' "${atffile}" | cut -d ' ' -f 2-)"

    for file in ${files} $(cd "$(dirname "${atffile}")" && echo ${globs}); do
        if test -d "${dir}/${file}" -o -x "${dir}/${file}"; then
            echo "${file}"
        fi
    done
}


# Converts an Atffile to a Kyuafile.
#
# \param atffile The path to the Atfffile to convert.
# \param kyuafile The path to where the Kyuafile will be written.
convert_atffile() {
    local atffile="${1}"; shift
    local kyuafile="${1}"; shift

    info "Converting ${atffile} -> ${kyuafile}"

    local test_suite="$(grep 'prop:.*test-suite.*' "${atffile}" \
        | cut -d \" -f 2)"

    local dir="$(dirname "${atffile}")"

    local subdirs=
    local test_programs=
    for file in $(extract_files "${atffile}"); do
        if test -f "${dir}/${file}/Atffile"; then
            subdirs="${subdirs} ${file}"
        elif test -x "${dir}/${file}"; then
            test_programs="${test_programs} ${file}"
        fi
    done

    echo "syntax('kyuafile', 1)" >"${kyuafile}"
    echo >>"${kyuafile}"
    echo "test_suite('${test_suite}')" >>"${kyuafile}"
    if [ -n "${subdirs}" ]; then
        echo >>"${kyuafile}"
        for dir in ${subdirs}; do
            echo "include('${dir}/Kyuafile')" >>"${kyuafile}"
        done
    fi
    if [ -n "${test_programs}" ]; then
        echo >>"${kyuafile}"
        for tp in ${test_programs}; do
            echo "atf_test_program{name='${tp}'}" >>"${kyuafile}"
        done
    fi
}


# Adds Kyuafiles to a test suite by converting any existing Atffiles.
#
# \param root The path to the test suite root.  Must contain an Atffile.
add_kyuafiles() {
    local root="${1}"; shift

    [ -f "${root}/Atffile" ] || error "${root} is not a test suite"

    for atffile in $(find "${root}" -name Atffile); do
        local kyuafile="$(echo "${atffile}" | sed s,Atffile,Kyuafile,)"
        convert_atffile "${atffile}" "${kyuafile}"
    done
}


# main root_directory
#
# Entry point for the test program.
main() {
    [ ${#} -eq 1 ] || usage_error "No test directory specified"
    local root="${1}"; shift

    remove_kyuafiles "${root}"
    add_kyuafiles "${root}"
}


main "${@}"
