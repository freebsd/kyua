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


utils_test_case premature_exit
premature_exit_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="bogus_test_cases"}
EOF

    cat >expout <<EOF
bogus_test_cases:die  ->  broken: Premature exit: received signal 6
bogus_test_cases:exit  ->  broken: Premature exit: exited with code 0
bogus_test_cases:pass  ->  passed

1/3 passed (2 failed)
EOF

    utils_cp_helper bogus_test_cases .
    atf_check -s exit:1 -o file:expout -e empty kyua test
}


utils_test_case no_args
no_args_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="simple_all_pass"}
include("subdir/Kyuafile")
EOF
    utils_cp_helper metadata .
    utils_cp_helper simple_all_pass .

    mkdir subdir
    cat >subdir/Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration2")
atf_test_program{name="simple_some_fail"}
EOF
    utils_cp_helper simple_some_fail subdir

    cat >expout <<EOF
simple_all_pass:pass  ->  passed
simple_all_pass:skip  ->  skipped: The reason for skipping is this
subdir/simple_some_fail:fail  ->  failed: This fails on purpose
subdir/simple_some_fail:pass  ->  passed

3/4 passed (1 failed)
EOF
    atf_check -s exit:1 -o file:expout -e empty kyua test
}


utils_test_case one_arg__subdir
one_arg__subdir_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("top-level")
include("subdir/Kyuafile")
EOF

    mkdir subdir
    cat >subdir/Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("in-subdir")
atf_test_program{name="simple_all_pass"}
EOF
    utils_cp_helper simple_all_pass subdir

    cat >expout <<EOF
subdir/simple_all_pass:pass  ->  passed
subdir/simple_all_pass:skip  ->  skipped: The reason for skipping is this

2/2 passed (0 failed)
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua test subdir
}


utils_test_case one_arg__test_case
one_arg__test_case_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("top-level")
atf_test_program{name="first"}
atf_test_program{name="second"}
EOF
    utils_cp_helper simple_all_pass first
    utils_cp_helper simple_all_pass second

    cat >expout <<EOF
first:skip  ->  skipped: The reason for skipping is this

1/1 passed (0 failed)
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua test first:skip
}


utils_test_case one_arg__test_program
one_arg__test_program_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("top-level")
atf_test_program{name="first"}
atf_test_program{name="second"}
EOF
    utils_cp_helper simple_all_pass first
    utils_cp_helper simple_some_fail second

    cat >expout <<EOF
second:fail  ->  failed: This fails on purpose
second:pass  ->  passed

1/2 passed (1 failed)
EOF
    atf_check -s exit:1 -o file:expout -e empty kyua test second
}


utils_test_case one_arg__invalid
one_arg__invalid_body() {
cat >experr <<EOF
kyua: E: Test case component in 'foo:' is empty.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua test foo:

cat >experr <<EOF
kyua: E: Program name '/a/b' must be relative to the test suite, not absolute.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua test /a/b
}


utils_test_case many_args__ok
many_args__ok_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("top-level")
include("subdir/Kyuafile")
atf_test_program{name="first"}
EOF
    utils_cp_helper simple_all_pass first

    mkdir subdir
    cat >subdir/Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("in-subdir")
atf_test_program{name="second"}
EOF
    utils_cp_helper simple_some_fail subdir/second

    cat >expout <<EOF
subdir/second:fail  ->  failed: This fails on purpose
subdir/second:pass  ->  passed
first:pass  ->  passed

2/3 passed (1 failed)
EOF
    atf_check -s exit:1 -o file:expout -e empty kyua test subdir first:pass
}


utils_test_case many_args__invalid
many_args__invalid_body() {
cat >experr <<EOF
kyua: E: Program name component in ':badbad' is empty.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua test this-is-ok :badbad

cat >experr <<EOF
kyua: E: Program name '/foo' must be relative to the test suite, not absolute.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua test this-is-ok /foo
}


utils_test_case many_args__no_match__all
many_args__no_match__all_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("top-level")
atf_test_program{name="first"}
atf_test_program{name="second"}
EOF
    utils_cp_helper simple_all_pass first
    utils_cp_helper simple_all_pass second

    cat >experr <<EOF
kyua: W: No test cases matched by the filter 'first1'.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua test first1
}


utils_test_case many_args__no_match__some
many_args__no_match__some_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("top-level")
atf_test_program{name="first"}
atf_test_program{name="second"}
atf_test_program{name="third"}
EOF
    utils_cp_helper simple_all_pass first
    utils_cp_helper simple_all_pass second
    utils_cp_helper simple_some_fail third

    cat >expout <<EOF
first:pass  ->  passed
first:skip  ->  skipped: The reason for skipping is this
third:fail  ->  failed: This fails on purpose
third:pass  ->  passed

3/4 passed (1 failed)
EOF

    cat >experr <<EOF
kyua: W: No test cases matched by the filter 'fifth'.
kyua: W: No test cases matched by the filter 'fourth'.
EOF
    atf_check -s exit:1 -o file:expout -e file:experr kyua test first fourth \
        third fifth
}


utils_test_case args_are_relative
args_are_relative_body() {
    mkdir root
    cat >root/Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration-1")
atf_test_program{name="first"}
atf_test_program{name="second"}
include("subdir/Kyuafile")
EOF
    utils_cp_helper simple_all_pass root/first
    utils_cp_helper simple_some_fail root/second

    mkdir root/subdir
    cat >root/subdir/Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration-2")
atf_test_program{name="third"}
atf_test_program{name="fourth"}
EOF
    utils_cp_helper simple_all_pass root/subdir/third
    utils_cp_helper simple_some_fail root/subdir/fourth

    cat >expout <<EOF
first:pass  ->  passed
first:skip  ->  skipped: The reason for skipping is this
subdir/fourth:fail  ->  failed: This fails on purpose

2/3 passed (1 failed)
EOF
    atf_check -s exit:1 -o file:expout -e empty kyua test \
        -k "$(pwd)/root/Kyuafile" first subdir/fourth:fail
}


utils_test_case only_load_used_test_programs
only_load_used_test_programs_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="first"}
atf_test_program{name="second"}
EOF
    utils_cp_helper simple_all_pass first
    utils_cp_helper bad_test_program second

    cat >expout <<EOF
first:pass  ->  passed
first:skip  ->  skipped: The reason for skipping is this

2/2 passed (0 failed)
EOF
    CREATE_COOKIE="$(pwd)/cookie"; export CREATE_COOKIE
    atf_check -s exit:0 -o file:expout -e empty kyua test first
    if test -f "${CREATE_COOKIE}"; then
        atf_fail "An unmatched test case has been executed, which harms" \
            "performance"
    fi
}


utils_test_case missing_test_program
missing_test_program_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="first"}
EOF
    utils_cp_helper simple_all_pass first
    utils_cp_helper simple_all_pass second

    cat >experr <<EOF
kyua: W: No test cases matched by the filter 'second'.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua test second
}


utils_test_case missing_test_case
missing_test_case_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="first"}
EOF
    utils_cp_helper simple_all_pass first

    cat >experr <<EOF
kyua: W: No test cases matched by the filter 'first:foobar'.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua test first:foobar
}


utils_test_case missing_kyuafile__no_args
missing_kyuafile__no_args_body() {
    cat >experr <<EOF
kyua: E: Load of 'Kyuafile' failed: File 'Kyuafile' not found.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua test
}


utils_test_case missing_kyuafile__test_program
missing_kyuafile__test_program_body() {
    mkdir subdir
    cat >subdir/Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="unused"}
EOF
    utils_cp_helper simple_all_pass subdir/unused

    cat >experr <<EOF
kyua: E: Load of 'Kyuafile' failed: File 'Kyuafile' not found.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua test subdir/unused
}


utils_test_case missing_kyuafile__subdir
missing_kyuafile__subdir_body() {
    mkdir subdir
    cat >subdir/Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="unused"}
EOF
    utils_cp_helper simple_all_pass subdir/unused

    cat >experr <<EOF
kyua: E: Load of 'Kyuafile' failed: File 'Kyuafile' not found.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua test subdir
}


utils_test_case bogus_config
bogus_config_body() {
    cat >"${HOME}/.kyuarc" <<EOF
Hello, world.
EOF

    atf_check -s exit:1 -o empty \
        -e match:"^kyua: E: Load of '.*kyuarc' failed: Failed to load Lua" \
        kyua test
}


utils_test_case bogus_kyuafile
bogus_kyuafile_body() {
    cat >Kyuafile <<EOF
Hello, world.
EOF

    cat >experr <<EOF
kyua: E: Load of 'Kyuafile' failed: Failed to load Lua file 'Kyuafile': Kyuafile:2: '<name>' expected near '<eof>'.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua test
}


utils_test_case bogus_test_program
bogus_test_program_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="bad_test_program"}
EOF
    utils_cp_helper bad_test_program .

    cat >expout <<EOF
bad_test_program:__test_program__  ->  broken: Failed to load list of test cases: bad_test_program: Invalid header for test case list; expecting Content-Type for application/X-atf-tp version 1, got 'This is not a valid test program!'

0/1 passed (1 failed)
EOF
    atf_check -s exit:1 -o file:expout -e empty kyua test
}


utils_test_case config__behavior
config__behavior_body() {
    cat >"${HOME}/.kyuarc" <<EOF
syntax("config", 1)
test_suite_var("suite1", "X-the-variable", "value1")
test_suite_var("suite2", "X-the-variable", "value2")
EOF

    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
atf_test_program{name="config1", test_suite="suite1"}
atf_test_program{name="config2", test_suite="suite2"}
atf_test_program{name="config3", test_suite="suite3"}
EOF
    utils_cp_helper config config1
    utils_cp_helper config config2
    utils_cp_helper config config3

    atf_check -s exit:1 -o save:stdout -e empty kyua test
    atf_check -s exit:0 -o ignore -e empty \
        grep 'config1:get_variable.*failed' stdout
    atf_check -s exit:0 -o ignore -e empty \
        grep 'config2:get_variable.*passed' stdout
    atf_check -s exit:0 -o ignore -e empty \
        grep 'config3:get_variable.*skipped' stdout
}


utils_test_case config_flag__default_system
config_flag__default_system_body() {
    cat >kyua.conf <<EOF
syntax("config", 1)
test_suite_var("integration", "X-the-variable", "value2")
EOF

    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="config"}
EOF
    utils_cp_helper config .

    atf_check -s exit:0 -o match:"get_variable.*skipped" -e empty kyua test
    export KYUA_CONFDIR="$(pwd)"
    atf_check -s exit:0 -o match:"get_variable.*passed" -e empty kyua test
}


utils_test_case config_flag__default_home
config_flag__default_home_body() {
    cat >kyuarc <<EOF
syntax("config", 1)
test_suite_var("integration", "X-the-variable", "value2")
EOF

    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="config"}
EOF
    utils_cp_helper config .

    atf_check -s exit:0 -o match:"get_variable.*skipped" -e empty kyua test
    mv kyuarc "${HOME}/.kyuarc"
    atf_check -s exit:0 -o match:"get_variable.*passed" -e empty kyua test
}


utils_test_case config_flag__explicit__ok
config_flag__explicit__ok_body() {
    cat >kyuarc <<EOF
syntax("config", 1)
test_suite_var("integration", "X-the-variable", "value2")
EOF

    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="config"}
EOF
    utils_cp_helper config .

    atf_check -s exit:0 -o match:"get_variable.*skipped" -e empty kyua test
    atf_check -s exit:0 -o match:"get_variable.*passed" -e empty kyua test \
        -c kyuarc
    atf_check -s exit:0 -o match:"get_variable.*passed" -e empty kyua test \
        --config=kyuarc
}


utils_test_case config_flag__explicit__missing_file
config_flag__explicit__missing_file_body() {
    cat >experr <<EOF
kyua: E: Load of 'foo' failed: File 'foo' not found.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua test --config=foo
}


utils_test_case config_flag__explicit__bad_file
config_flag__explicit__bad_file_body() {
    touch custom
    atf_check -s exit:1 -o empty -e match:"Syntax not defined.*'custom'" \
        kyua test --config=custom
}


utils_test_case variable_flag__no_config
variable_flag__no_config_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
atf_test_program{name="config1", test_suite="suite1"}
atf_test_program{name="config2", test_suite="suite2"}
atf_test_program{name="config3", test_suite="suite3"}
EOF
    utils_cp_helper config config1
    utils_cp_helper config config2
    utils_cp_helper config config3

    check_stdout() {
        atf_check -s exit:0 -o ignore -e empty \
            grep 'config1:get_variable.*failed' stdout
        atf_check -s exit:0 -o ignore -e empty \
            grep 'config2:get_variable.*passed' stdout
        atf_check -s exit:0 -o ignore -e empty \
            grep 'config3:get_variable.*skipped' stdout
    }

    atf_check -s exit:1 -o save:stdout -e empty kyua test \
        -v "suite1.X-the-variable=value1" \
        -v "suite2.X-the-variable=value2"
    check_stdout

    atf_check -s exit:1 -o save:stdout -e empty kyua test \
        --variable="suite1.X-the-variable=value1" \
        --variable="suite2.X-the-variable=value2"
    check_stdout
}


utils_test_case variable_flag__override_default_config
variable_flag__override_default_config_body() {
    cat >"${HOME}/.kyuarc" <<EOF
syntax("config", 1)
test_suite_var("suite1", "X-the-variable", "value1")
test_suite_var("suite2", "X-the-variable", "should not be used")
EOF

    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
atf_test_program{name="config1", test_suite="suite1"}
atf_test_program{name="config2", test_suite="suite2"}
atf_test_program{name="config3", test_suite="suite3"}
atf_test_program{name="config4", test_suite="suite4"}
EOF
    utils_cp_helper config config1
    utils_cp_helper config config2
    utils_cp_helper config config3
    utils_cp_helper config config4

    check_stdout() {
        atf_check -s exit:0 -o ignore -e empty \
            grep 'config1:get_variable.*failed' stdout
        atf_check -s exit:0 -o ignore -e empty \
            grep 'config2:get_variable.*passed' stdout
        atf_check -s exit:0 -o ignore -e empty \
            grep 'config3:get_variable.*skipped' stdout
        atf_check -s exit:0 -o ignore -e empty \
            grep 'config4:get_variable.*passed' stdout
    }

    atf_check -s exit:1 -o save:stdout -e empty kyua test \
        -v "suite2.X-the-variable=value2" \
        -v "suite4.X-the-variable=value2"
    check_stdout

    atf_check -s exit:1 -o save:stdout -e empty kyua test \
        --variable="suite2.X-the-variable=value2" \
        --variable="suite4.X-the-variable=value2"
    check_stdout
}


utils_test_case variable_flag__override_custom_config
variable_flag__override_custom_config_body() {
    cat >config <<EOF
syntax("config", 1)
test_suite_var("suite1", "X-the-variable", "value1")
test_suite_var("suite2", "X-the-variable", "should not be used")
EOF

    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
atf_test_program{name="config1", test_suite="suite1"}
atf_test_program{name="config2", test_suite="suite2"}
EOF
    utils_cp_helper config config1
    utils_cp_helper config config2

    check_stdout() {
        atf_check -s exit:0 -o ignore -e empty \
            grep 'config1:get_variable.*failed' stdout
        atf_check -s exit:0 -o ignore -e empty \
            grep 'config2:get_variable.*passed' stdout
    }

    atf_check -s exit:1 -o save:stdout -e empty kyua test -c config \
        -v "suite2.X-the-variable=value2"
    check_stdout

    atf_check -s exit:1 -o save:stdout -e empty kyua test -c config \
        --variable="suite2.X-the-variable=value2"
    check_stdout
}


utils_test_case variable_flag__invalid
variable_flag__invalid_body() {
    cat >experr <<EOF
Usage error for command test: Invalid argument '' for option --variable: Argument does not have the form 'name=value'.
Type 'kyua help test' for usage information.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua test \
        -v "a.b=c" -v ""

    cat >experr <<EOF
kyua: E: Unrecognized configuration property 'foo' in override 'foo=bar'.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua test \
        -v "a.b=c" -v "foo=bar"
}


utils_test_case kyuafile_flag__no_args
kyuafile_flag__no_args_body() {
    cat >Kyuafile <<EOF
This file is bogus but it is not loaded.
EOF

    cat >myfile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="sometest"}
EOF
    utils_cp_helper simple_all_pass sometest

    cat >expout <<EOF
sometest:pass  ->  passed
sometest:skip  ->  skipped: The reason for skipping is this

2/2 passed (0 failed)
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua test -k myfile
    atf_check -s exit:0 -o file:expout -e empty kyua test --kyuafile=myfile
}


utils_test_case kyuafile_flag__some_args
kyuafile_flag__some_args_body() {
    cat >Kyuafile <<EOF
This file is bogus but it is not loaded.
EOF

    cat >myfile <<EOF
syntax("kyuafile", 1)
test_suite("hello-world")
atf_test_program{name="sometest"}
EOF
    utils_cp_helper simple_all_pass sometest

    cat >expout <<EOF
sometest:pass  ->  passed
sometest:skip  ->  skipped: The reason for skipping is this

2/2 passed (0 failed)
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua test -k myfile sometest
    atf_check -s exit:0 -o file:expout -e empty kyua test --kyuafile=myfile \
        sometest
}


utils_test_case interrupt
interrupt_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="interrupts"}
EOF
    utils_cp_helper interrupts .

    kyua test \
        -v integration.X-body-cookie="$(pwd)/body" \
        -v integration.X-cleanup-cookie="$(pwd)/cleanup" \
        >stdout 2>stderr &
    pid=${!}

    while [ ! -f body ]; do
        echo "Waiting for body to start"
        sleep 1
    done
    sleep 1

    kill -INT ${pid}
    wait ${pid}
    [ ${?} -ne 0 ] || atf_fail 'No error code reported'

    [ -f cleanup ] || atf_fail 'Cleanup part not executed after signal'

    atf_check -s exit:0 -o ignore -e empty grep 'Signal caught' stderr
    atf_check -s exit:0 -o ignore -e empty \
        grep 'kyua: E: Interrupted by signal' stderr
}


atf_init_test_cases() {
    atf_add_test_case one_test_program__all_pass
    atf_add_test_case one_test_program__some_fail
    atf_add_test_case many_test_programs__all_pass
    atf_add_test_case many_test_programs__some_fail
    atf_add_test_case expect__all_pass
    atf_add_test_case expect__some_fail
    atf_add_test_case premature_exit

    atf_add_test_case no_args
    atf_add_test_case one_arg__subdir
    atf_add_test_case one_arg__test_case
    atf_add_test_case one_arg__test_program
    atf_add_test_case one_arg__invalid
    atf_add_test_case many_args__ok
    atf_add_test_case many_args__invalid
    atf_add_test_case many_args__no_match__all
    atf_add_test_case many_args__no_match__some

    atf_add_test_case args_are_relative

    atf_add_test_case only_load_used_test_programs

    atf_add_test_case config__behavior
    atf_add_test_case config_flag__default_system
    atf_add_test_case config_flag__default_home
    atf_add_test_case config_flag__explicit__ok
    atf_add_test_case config_flag__explicit__missing_file
    atf_add_test_case config_flag__explicit__bad_file
    atf_add_test_case variable_flag__no_config
    atf_add_test_case variable_flag__override_default_config
    atf_add_test_case variable_flag__override_custom_config
    atf_add_test_case variable_flag__invalid

    atf_add_test_case kyuafile_flag__no_args
    atf_add_test_case kyuafile_flag__some_args

    atf_add_test_case interrupt

    atf_add_test_case missing_test_program
    atf_add_test_case missing_test_case

    atf_add_test_case missing_kyuafile__no_args
    atf_add_test_case missing_kyuafile__test_program
    atf_add_test_case missing_kyuafile__subdir

    atf_add_test_case bogus_config
    atf_add_test_case bogus_kyuafile
    atf_add_test_case bogus_test_program
}
