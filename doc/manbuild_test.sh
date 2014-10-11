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


# Absolute path to the uninstalled script.
MANBUILD="__MANBUILD__"


atf_test_case empty
empty_body() {
    touch input
    atf_check "${MANBUILD}" input output
    atf_check cat output
}


atf_test_case no_replacements
no_replacements_body() {
    cat >input <<EOF
This is a manpage.

With more than one line.
EOF
    atf_check "${MANBUILD}" input output
    atf_check -o file:input cat output
}


atf_test_case some_replacements
some_replacements_body() {
    cat >input <<EOF
This is a manpage.
Where __FOO__ gets replaced.
And nothing more.
EOF
    atf_check "${MANBUILD}" -e 's,__FOO__,this,g' input output
    cat >expout <<EOF
This is a manpage.
Where this gets replaced.
And nothing more.
EOF
    atf_check -o file:expout cat output
}


atf_test_case generate_fail
generate_fail_body() {
    touch input
    atf_check -s exit:1 -o ignore \
        -e match:"manbuild.sh: Failed to generate output" \
        "${MANBUILD}" -e 's,malformed expression' input output
    [ ! -f output ] || atf_fail "Output file was generated but it should" \
        "not have been"
}


atf_test_case validate_fail
validate_fail_body() {
    cat >input <<EOF
This is a manpage.
Where __FOO__ gets replaced.
But where __BAR__ doesn't.
EOF
    atf_check -s exit:1 -o ignore \
        -e match:"manbuild.sh: Failed to generate output.*left unreplaced" \
        "${MANBUILD}" -e 's,__FOO__,this,g' input output
    [ ! -f output ] || atf_fail "Output file was generated but it should" \
        "not have been"
}


atf_test_case bad_args
bad_args_body() {
    atf_check -s exit:1 \
        -e match:'manbuild.sh: Must provide input and output names' \
        "${MANBUILD}"

    atf_check -s exit:1 \
        -e match:'manbuild.sh: Must provide input and output names' \
        "${MANBUILD}" foo

    atf_check -s exit:1 \
        -e match:'manbuild.sh: Must provide input and output names' \
        "${MANBUILD}" foo bar baz
}


atf_test_case bad_option
bad_option_body() {
    atf_check -s exit:1 -e match:'manbuild.sh: Unknown option -Z' \
        "${MANBUILD}" -Z
}


atf_init_test_cases() {
    atf_add_test_case empty
    atf_add_test_case no_replacements
    atf_add_test_case some_replacements
    atf_add_test_case generate_fail
    atf_add_test_case validate_fail
    atf_add_test_case bad_args
    atf_add_test_case bad_option
}
