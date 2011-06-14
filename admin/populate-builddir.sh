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

#
# This script mirrors a collection of files from the source directory
# into the build directory, respecting the layout the files had in the
# source tree.  The script is used to copy some files into the build
# directory in those cases where the build directory is different to
# the source directory.
#
# The files mirrored by this script are required to make 'distcheck'
# work: when using a build directory, some files must be present on it
# to let 'kyua test' find its control files.
#
# TODO(jmmv): Ideally, this should be expressed as build rules somehow.
# Unfortunately, I haven't been able to do this.  Rules such as
# 'foo: $(srcdir)/foo' (note that the file name is exactly the same in
# the target and in the dependency) do not work because of the VPATH
# setting that automake performs: make is unable to tell that the former
# 'foo' is different to its dependency.
#

set -e

Prog_Name=${0##*/}


recursive_copy() {
    local srcdir="${1}"; shift
    local builddir="${1}"; shift
    local ctlfile="${1}"; shift
    local basename="${1}"; shift

    local files="$(cd ${srcdir} && find . -name "${basename}" |
                   sed -e 's,^\./,,')"
    for file in ${files}; do
        if cmp -s "${srcdir}/${file}" "${builddir}/${file}"; then
            :
        else
            echo "${Prog_Name}: ${srcdir}/${file} -> ${builddir}/${file}"
            cp ${srcdir}/${file} ${builddir}/${file}
            echo "${builddir}/${file}" >>${ctlfile}
        fi
    done
}


main() {
    local srcdir="${1}"; shift
    local builddir="${1}"; shift
    local ctlfile="${1}"; shift

    rm -f "${ctlfile}"

    recursive_copy "${srcdir}" "${builddir}" "${ctlfile}" Atffile
    recursive_copy "${srcdir}" "${builddir}" "${ctlfile}" Kyuafile
}


main "${@}"
