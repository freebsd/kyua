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


# Subcommand to strip out the durations in a report.
#
# This is a subset of utils_strip_timestamps; see the documentation of that
# variable for details.
utils_strip_durations='sed -E \
    -e "s,( |\[|\")[0-9][0-9]*.[0-9][0-9][0-9](s]|s|\"),\1S.UUU\2,g"'


# Subcommand to strip out the durations and timestamps in a report.
#
# This is to make the reports deterministic and thus easily testable.  The
# time deltas are replaced by the fixed string S.UUU and the timestamps are
# replaced by the fixed string YYYYMMDD.HHMMSS.ssssss.
#
# This variable should be used as shown here:
#
#     atf_check ... -x kyua report "| ${utils_strip_timestamp}"
#
# Use the utils_install_timestamp_wrapper function to create a 'kyua' wrapper
# script that automatically does this.
utils_strip_timestamps='sed -E \
    -e "s,( |\[|\")[0-9][0-9]*.[0-9][0-9][0-9](s]|s|\"),\1S.UUU\2,g" \
    -e "s,[0-9]{8}-[0-9]{6}-[0-9]{6}\.db,YYYYMMDD-HHMMSS-ssssss.db,g"'


# Computes the action file for a test suite.
#
# \param path Optional path to use; if not given, use the cwd.
utils_action_file() {
    local test_suite_id="$(utils_test_suite_id "${@}")"
    echo "${HOME}/.kyua/actions/kyua.${test_suite_id}.YYYYMMDD-HHMMSS-ssssss.db"
}


# Copies a helper binary from the source directory to the work directory.
#
# \param name The name of the binary to copy.
# \param destination The target location for the binary; can be either
#     a directory name or a file name.
utils_cp_helper() {
    local name="${1}"; shift
    local destination="${1}"; shift

    ln -s "$(atf_get_srcdir)"/helpers/"${name}" "${destination}"
}


# Creates a 'kyua' binary in the path that strips durations off the output.
#
# Call this on test cases that wish to replace timestamps in the *stdout* of
# Kyua with the S.UUUs deterministic string.  This is to be used by tests that
# validate the 'test' subcommand, but also by a few specific tests for the
# 'report' subcommand.
utils_install_durations_wrapper() {
    [ ! -x kyua ] || return
    cat >kyua <<EOF
#! /bin/sh

PATH=${PATH}

kyua "\${@}" >kyua.tmpout
result=\${?}
cat kyua.tmpout | ${utils_strip_durations}
exit \${result}
EOF
    chmod +x kyua
    PATH="$(pwd):${PATH}"
}


# Creates a 'kyua' binary in the path that strips timestamps off the output.
#
# Call this on test cases that wish to replace durations and timestamps with a
# deterministic string.  This is to be used by tests that validate the 'test'
# subcommand, but also by a few specific tests for the 'report' subcommand.
utils_install_timestamp_wrapper() {
    [ ! -x kyua ] || return
    cat >kyua <<EOF
#! /bin/sh

PATH=${PATH}

kyua "\${@}" >kyua.tmpout
result=\${?}
cat kyua.tmpout | ${utils_strip_timestamps}
exit \${result}
EOF
    chmod +x kyua
    PATH="$(pwd):${PATH}"
}


# Defines a test case with a default head.
utils_test_case() {
    local name="${1}"; shift

    atf_test_case "${name}"
    eval "${name}_head() {
        atf_set require.progs kyua
    }"
}


# Computes the test suite identifier for action files.
#
# \param path Optional path to use; if not given, use the cwd.
utils_test_suite_id() {
    local path=
    if [ ${#} -gt 0 ]; then
        path="$(cd ${1} && pwd)"; shift
    else
        path="$(pwd)"
    fi
    echo "${path}" | sed -e 's,^/,,' -e 's,/,_,g'
}
