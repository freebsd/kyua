# Copyright 2012 Google Inc.
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


# Executes a mock test suite to generate data in the database.
#
# \param mock_env The value to store in a MOCK variable in the environment.
#     Use this to be able to differentiate executions by inspecting the
#     context of the output.
#
# \return The action identifier of the committed action.
run_tests() {
    local mock_env="${1}"

    mkdir testsuite
    cd testsuite

    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="simple_all_pass"}
EOF

    utils_cp_helper simple_all_pass .
    test -d ../.kyua || mkdir ../.kyua
    kyua=$(which kyua)
    atf_check -s exit:0 -o save:stdout -e empty env \
        HOME="$(pwd)/home" MOCK="${mock_env}" \
        "${kyua}" test --store=../.kyua/store.db

    action_id=$(grep '^Committed action ' stdout | cut -d ' ' -f 3)
    echo "New action is ${action_id}"

    cd -
    # Ensure the results of 'report' come from the database.
    rm -rf testsuite

    return "${action_id}"
}


utils_test_case default_behavior__ok
default_behavior__ok_body() {
    utils_install_timestamp_wrapper

    run_tests "mock1"

    atf_check -s exit:0 -o ignore -e empty kyua report-html
    test -f html/index.html || atf_fail "Missing index.html"
    test -f html/context.html || atf_fail "Missing context.html"
    test -f html/simple_all_pass_pass.html || atf_fail "Missing test file"
    test -f html/simple_all_pass_skip.html || atf_fail "Missing test file"

    grep "ALL TESTS PASSING" html/index.html || atf-fail "Bad result"
}


utils_test_case default_behavior__no_actions
default_behavior__no_actions_body() {
    kyua db-exec "SELECT * FROM actions"

    echo 'kyua: E: No actions in the database.' >experr
    atf_check -s exit:1 -o empty -e file:experr kyua report-html
}


utils_test_case default_behavior__no_store
default_behavior__no_store_body() {
    atf_check -s exit:1 -o empty \
        -e match:"kyua: E: Cannot open '.*/.kyua/store.db': " kyua report-html
}


utils_test_case action__explicit
action__explicit_body() {
    run_tests "mock1"; action1=$?
    run_tests "mock2"; action2=$?

    atf_check -s exit:0 -o ignore -e empty kyua report-html \
        --action="${action1}"
    grep "action 1" html/index.html || atf_fail "Invalid action in report"
    grep "MOCK.*mock1" html/context.html || atf_fail "Invalid context in report"

    rm -rf html
    atf_check -s exit:0 -o ignore -e empty kyua report-html \
        --action="${action2}"
    grep "action 2" html/index.html || atf_fail "Invalid action in report"
    grep "MOCK.*mock2" html/context.html || atf_fail "Invalid context in report"
}


utils_test_case action__not_found
action__not_found_body() {
    kyua db-exec "SELECT * FROM actions"

    echo 'kyua: E: Error loading action 514: does not exist.' >experr
    atf_check -s exit:1 -o empty -e file:experr kyua report-html --action=514
}


utils_test_case force__yes
force__yes_body() {
    run_tests "mock1"

    atf_check -s exit:0 -o ignore -e empty kyua report-html
    test -f html/index.html || atf_fail "Expected file not created"
    rm html/index.html
    atf_check -s exit:0 -o ignore -e empty kyua report-html --force
    test -f html/index.html || atf_fail "Expected file not created"
}


utils_test_case force__no
force__no_body() {
    run_tests "mock1"

    atf_check -s exit:0 -o ignore -e empty kyua report-html
    test -f html/index.html || atf_fail "Expected file not created"
    rm html/index.html

cat >experr <<EOF
kyua: E: Output directory 'html' already exists; maybe use --force?.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua report-html
    test ! -f html/index.html || atf_fail "Not expected file created"
}


utils_test_case output__explicit
output__explicit_body() {
    run_tests "mock1"

    mkdir output
    atf_check -s exit:0 -o ignore -e empty kyua report-html --output=output/foo
    test ! -d html || atf_fail "Not expected directory created"
    test -f output/foo/index.html || atf_fail "Expected file not created"
}


atf_init_test_cases() {
    atf_add_test_case default_behavior__ok
    atf_add_test_case default_behavior__no_actions
    atf_add_test_case default_behavior__no_store

    atf_add_test_case action__explicit
    atf_add_test_case action__not_found

    atf_add_test_case force__yes
    atf_add_test_case force__no

    atf_add_test_case output__explicit
}
