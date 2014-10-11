#! /bin/sh
# Copyright 2014 Google Inc.
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

# \file doc/manbuild.sh
# Generates a manual page from a source file.


# Name of the running program for error reporting purposes.
Prog_Name="${0##*/}"


# Prints an error message and exits.
#
# Args:
#   ...: The error message to print.  Multiple arguments are joined with a
#       single space separator.
err() {
    echo "${Prog_Name}: ${*}" 1>&2
    exit 1
}


# Generates the manual page reading from stdin and dumping to stdout.
#
# Args:
#   ...: Arguments to pass to sed.
#
# Returns:
#   True if the generation succeeds; false otherwise.
generate() {
    sed "${@}"
}


# Validates that the manual page has been properly generated.
#
# In particular, this checks if any directives or common replacement patterns
# have been left in place.
#
# Returns:
#   True if the manual page is valid; false otherwise.
validate_file() {
    local filename="${1}"

    if grep '__[A-Za-z0-9]*__' "${filename}" >/dev/null; then
        return 1
    else
        return 0
    fi
}


# Program entry point.
main() {
    local sed_expressions=

    while getopts :e: arg; do
        case "${arg}" in
            e)
                sed_expressions="${sed_expressions} -e${OPTARG}"
                ;;

            \?)
                err "Unknown option -${OPTARG}"
                ;;
        esac
    done
    shift $((${OPTIND} - 1))

    [ ${#} -eq 2 ] || err "Must provide input and output names as arguments"
    local input="${1}"; shift
    local output="${1}"; shift

    trap "rm -f '${output}.tmp'" EXIT HUP INT TERM
    generate ${sed_expressions} <"${input}" >"${output}.tmp" \
        || err "Failed to generate ${output}"
    if validate_file "${output}.tmp"; then
        :
    else
        err "Failed to generate ${output}; some patterns were left unreplaced"
    fi
    mv "${output}.tmp" "${output}"
}


main "${@}"
