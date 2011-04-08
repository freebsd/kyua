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


utils_test_case one_test_program__all_pass
one_test_program__all_pass_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="simple_all_pass"}
EOF

    cat >expout <<EOF
simple_all_pass:pass  ->  passed
simple_all_pass:skip  ->  skipped: The reason for skipping is this

2/2 passed (0 failed)
EOF

    utils_cp_helper simple_all_pass .
    atf_check -s exit:0 -o file:expout -e empty kyua test
}


utils_test_case one_test_program__some_fail
one_test_program__some_fail_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="simple_some_fail"}
EOF

    cat >expout <<EOF
simple_some_fail:fail  ->  failed: This fails on purpose
simple_some_fail:pass  ->  passed

1/2 passed (1 failed)
EOF

    utils_cp_helper simple_some_fail .
    atf_check -s exit:1 -o file:expout -e empty kyua test
}


utils_test_case many_test_programs__all_pass
many_test_programs__all_pass_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="first"}
atf_test_program{name="second"}
atf_test_program{name="third"}
EOF

    cat >expout <<EOF
first:pass  ->  passed
first:skip  ->  skipped: The reason for skipping is this
second:pass  ->  passed
second:skip  ->  skipped: The reason for skipping is this
third:pass  ->  passed
third:skip  ->  skipped: The reason for skipping is this

6/6 passed (0 failed)
EOF

    utils_cp_helper simple_all_pass first
    utils_cp_helper simple_all_pass second
    utils_cp_helper simple_all_pass third
    atf_check -s exit:0 -o file:expout -e empty kyua test
}


utils_test_case many_test_programs__some_fail
many_test_programs__some_fail_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="first"}
atf_test_program{name="second"}
atf_test_program{name="third"}
EOF

    cat >expout <<EOF
first:fail  ->  failed: This fails on purpose
first:pass  ->  passed
second:fail  ->  failed: This fails on purpose
second:pass  ->  passed
third:pass  ->  passed
third:skip  ->  skipped: The reason for skipping is this

4/6 passed (2 failed)
EOF

    utils_cp_helper simple_some_fail first
    utils_cp_helper simple_some_fail second
    utils_cp_helper simple_all_pass third
    atf_check -s exit:1 -o file:expout -e empty kyua test
}


utils_test_case expect__all_pass
expect__all_pass_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="expect_all_pass"}
EOF

    cat >expout <<EOF
expect_all_pass:die  ->  expected_death: This is the reason for death
expect_all_pass:exit  ->  expected_exit(12): Exiting with correct code
expect_all_pass:failure  ->  expected_failure: Oh no: Forced failure
expect_all_pass:signal  ->  expected_signal(15): Exiting with correct signal
expect_all_pass:timeout  ->  expected_timeout: This times out

5/5 passed (0 failed)
EOF

    utils_cp_helper expect_all_pass .
    atf_check -s exit:0 -o file:expout -e empty kyua test
}


utils_test_case expect__some_fail
expect__some_fail_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="expect_some_fail"}
EOF

    cat >expout <<EOF
expect_some_fail:die  ->  failed: Test case was expected to terminate abruptly but it continued execution
expect_some_fail:exit  ->  broken: Expected clean exit with code 12 but got code 34
expect_some_fail:failure  ->  failed: Test case was expecting a failure but none were raised
expect_some_fail:pass  ->  passed
expect_some_fail:signal  ->  broken: Expected signal 15 but got 9
expect_some_fail:timeout  ->  failed: Test case was expected to hang but it continued execution

1/6 passed (5 failed)
EOF

    utils_cp_helper expect_some_fail .
    atf_check -s exit:1 -o file:expout -e empty kyua test
}


utils_test_case subdirs
subdirs_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="top"}
include("dir/CustomKyuafile")
EOF

    mkdir dir
    cat >dir/CustomKyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="nested"}
EOF

    cat >expout <<EOF
top:pass  ->  passed
top:skip  ->  skipped: The reason for skipping is this
dir/nested:fail  ->  failed: This fails on purpose
dir/nested:pass  ->  passed

3/4 passed (1 failed)
EOF

    utils_cp_helper simple_all_pass top
    utils_cp_helper simple_some_fail dir/nested
    atf_check -s exit:1 -o file:expout -e empty kyua test
}


atf_init_test_cases() {
    atf_add_test_case one_test_program__all_pass
    atf_add_test_case one_test_program__some_fail

    atf_add_test_case many_test_programs__all_pass
    atf_add_test_case many_test_programs__some_fail

    atf_add_test_case expect__all_pass
    atf_add_test_case expect__some_fail

    atf_add_test_case subdirs

    # TODO(jmmv): Add test cases for:
    # - Failures: this requires code changes to pretty-print the error messages.
    # - Flags: both -k and -c, in long and short versions.
}
