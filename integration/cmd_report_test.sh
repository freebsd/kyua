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
    atf_check -s exit:0 -o save:stdout -e empty env -i \
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
    run_tests "mock1"

    cat >expout <<EOF
===> Skipped tests
$(pwd)/testsuite/simple_all_pass:skip  ->  skipped: The reason for skipping is this
===> Summary
Action: 1
Test cases: 2 total, 1 skipped, 0 expected failures, 0 broken, 0 failed
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua report

    run_tests "mock2"

    cat >expout <<EOF
===> Skipped tests
$(pwd)/testsuite/simple_all_pass:skip  ->  skipped: The reason for skipping is this
===> Summary
Action: 2
Test cases: 2 total, 1 skipped, 0 expected failures, 0 broken, 0 failed
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua report
}


utils_test_case default_behavior__no_actions
default_behavior__no_actions_body() {
    kyua db-exec "SELECT * FROM actions"

    echo 'kyua: E: No actions in the database.' >experr
    atf_check -s exit:1 -o empty -e file:experr kyua report
}


utils_test_case default_behavior__no_store
default_behavior__no_store_body() {
    atf_check -s exit:1 -o empty \
        -e match:"kyua: E: Cannot open '.*/.kyua/store.db': " kyua report
}


utils_test_case action__explicit
action__explicit_body() {
    run_tests "mock1"; action1=$?
    run_tests "mock2"; action2=$?

    atf_check -s exit:0 -o match:"MOCK=mock1" -o not-match:"MOCK=mock2" \
        -o match:"Action: 1" -o not-match:"Action: 2" \
        -e empty kyua report --action="${action1}" --show-context
    atf_check -s exit:0 -o not-match:"MOCK=mock1" -o match:"MOCK=mock2" \
        -o match:"Action: 2" -o not-match:"Action: 1" \
        -e empty kyua report --action="${action2}" --show-context
}


utils_test_case action__not_found
action__not_found_body() {
    kyua db-exec "SELECT * FROM actions"

    echo 'kyua: E: Error loading action 514: does not exist.' >experr
    atf_check -s exit:1 -o empty -e file:experr kyua report --action=514
}


utils_test_case show_context
show_context_body() {
    run_tests "mock1"

    cat >expout <<EOF
===> Execution context
Current directory: $(pwd)/testsuite
Environment variables:
    HOME=$(pwd)/testsuite/home
    MOCK=mock1
===> Skipped tests
$(pwd)/testsuite/simple_all_pass:skip  ->  skipped: The reason for skipping is this
===> Summary
Action: 1
Test cases: 2 total, 1 skipped, 0 expected failures, 0 broken, 0 failed
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua report --show-context
}


utils_test_case output__console__change_file
output__console__change_file_body() {
    run_tests

    cat >experr <<EOF
===> Skipped tests
$(pwd)/testsuite/simple_all_pass:skip  ->  skipped: The reason for skipping is this
===> Summary
Action: 1
Test cases: 2 total, 1 skipped, 0 expected failures, 0 broken, 0 failed
EOF
    atf_expect_fail "--output not implemented yet"
    atf_check -s exit:0 -o empty -e file:experr kyua report \
        --output=console:/dev/stderr
}


atf_init_test_cases() {
    atf_add_test_case default_behavior__ok
    atf_add_test_case default_behavior__no_actions
    atf_add_test_case default_behavior__no_store

    atf_add_test_case action__explicit
    atf_add_test_case action__not_found

    atf_add_test_case show_context

    atf_add_test_case output__console__change_file
}
