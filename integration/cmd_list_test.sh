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


utils_test_case no_args
no_args_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="metadata"}
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
metadata:no_properties
metadata:one_property
metadata:many_properties
metadata:with_cleanup
simple_all_pass:pass
simple_all_pass:skip
subdir/simple_some_fail:fail
subdir/simple_some_fail:pass
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua list
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
subdir/simple_all_pass:pass
subdir/simple_all_pass:skip
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua list subdir
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
first:skip
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua list \
        first:skip
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
second:fail
second:pass
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua list second
}


utils_test_case one_arg__invalid
one_arg__invalid_body() {
cat >experr <<EOF
Usage error for command list: Test case component in 'foo:' is empty.
Type 'kyua help list' for usage information.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua list foo:
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
subdir/second:fail
    test-suite = in-subdir
subdir/second:pass
    test-suite = in-subdir
first:pass
    test-suite = top-level
EOF
    atf_expect_fail "-v not implemented yet"
    atf_check -s exit:0 -o file:expout -e empty kyua list -v subdir first:pass
}


utils_test_case many_args__invalid
many_args__invalid_body() {
cat >experr <<EOF
Usage error for command list: Program name component in ':badbad' is empty.
Type 'kyua help list' for usage information.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua list this-is-ok :badbad
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
No test cases matched by the filters provided.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua list first1
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
first:pass
first:skip
third:fail
third:pass
EOF

    cat >experr <<EOF
No test cases matched by the 'fourth' filter.
EOF
    atf_expect_fail "Validation of individual filters not implemented"
    atf_check -s exit:1 -o empty -e file:experr kyua list first fourth third
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
first:pass
    test-suite = integration-1
first:skip
    test-suite = integration-1
subdir/fourth:fail
    test-suite = integration-2
EOF
    atf_expect_fail "Test names are not yet relative"
    atf_check -s exit:0 -o file:expout -e empty kyua list \
        -v -k "$(pwd)/root/Kyuafile" first subdir/fourth:fail

cat >experr <<EOF
Test case not found.  TODO(jmmv): Adjust this expected message.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua list \
        -v -k "$(pwd)/root/Kyuafile" "$(pwd)/root/first"
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
first:pass
first:skip
EOF
    CREATE_COOKIE="$(pwd)/cookie"; export CREATE_COOKIE
    atf_check -s exit:0 -o file:expout -e empty kyua list first
    if test -f "${CREATE_COOKIE}"; then
        atf_fail "An unmatched test case has been executed, which harms" \
            "performance"
    fi
}

utils_test_case verbose_flag
verbose_flag_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration-suite-1")
atf_test_program{name="simple_all_pass"}
include("subdir/Kyuafile")
EOF
    utils_cp_helper simple_all_pass .

    mkdir subdir
    cat >subdir/Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration-suite-2")
atf_test_program{name="metadata"}
EOF
    utils_cp_helper metadata subdir

    cat >expout <<EOF
simple_all_pass:pass
    test-suite = integration-suite-1
simple_all_pass:skip
    test-suite = integration-suite-1
subdir/metadata:no_properties
    test-suite = integration-suite-2
subdir/metadata:one_property
    test-suite = integration-suite-2
    descr = Does nothing but has one metadata property
subdir/metadata:many_properties
    test-suite = integration-suite-2
    descr =     A description with some padding
    require.arch = some-architecture
    require.config = var1 var2 var3
    require.machine = some-platform
    require.progs = bin1 bin2 /nonexistent/bin3
    require.user = root
    X-no-meaning = I am a custom variable
subdir/metadata:with_cleanup
    test-suite = integration-suite-2
    has.cleanup = true
    timeout = 300
EOF
    atf_expect_fail "Printing of properties not implemented"
    atf_check -s exit:0 -o file:expout -e empty kyua list -v
    atf_check -s exit:0 -o file:expout -e empty kyua list --verbose
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
Test program not found.  TODO(jmmv): Adjust this expected message.
EOF
    atf_expect_fail "Test program presence does not obey the Kyuafile"
    atf_check -s exit:1 -o empty -e file:experr kyua list second
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
Test case not found.  TODO(jmmv): Adjust this expected message.
EOF
    atf_expect_fail "Test case listing not implemented"
    atf_check -s exit:1 -o empty -e file:experr kyua list first:foobar
}


utils_test_case missing_kyuafile__no_args
missing_kyuafile__no_args_body() {
    cat >experr <<EOF
File 'Kyuafile' not found.  TODO(jmmv): Adjust this expected message.
EOF

    atf_expect_fail "Missing Kyuafile error not captured"
    atf_check -s exit:1 -o empty -e file:experr kyua list
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
File 'Kyuafile' not found.  TODO(jmmv): Adjust this expected message.
EOF
    atf_expect_fail "Providing arguments incorrectly skip load of Kyuafiles"
    atf_check -s exit:1 -o empty -e file:experr kyua list subdir/unused
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
File 'Kyuafile' not found.  TODO(jmmv): Adjust this expected message.
EOF
    atf_expect_fail "Kyuafile in subdir cannot load without parent"
    atf_check -s exit:1 -o empty -e file:experr kyua list subdir
}


utils_test_case bogus_kyuafile
bogus_kyuafile_body() {
    cat >Kyuafile <<EOF
Hello, world.
EOF

    cat >experr <<EOF
Bad 'Kyuafile'.  TODO(jmmv): Adjust this expected message.
EOF
    atf_expect_fail "Bad kyuafile error not captured"
    atf_check -s exit:1 -o empty -e file:experr kyua list
}


utils_test_case bogus_test_program
bogus_test_program_body() {
    cat >Kyuafile <<EOF
syntax("kyuafile", 1)
test_suite("integration")
atf_test_program{name="bad_test_program"}
EOF
    utils_cp_helper bad_test_program .

    cat >experr <<EOF
Bad test program.  TODO(jmmv): Adjust this expected message.
EOF
    atf_expect_fail "Bogus test program errors not captured"
    atf_check -s exit:1 -o empty -e file:experr kyua list
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
sometest:pass
sometest:skip
EOF
    atf_check -s exit:0 -o file:expout -e empty kyua list -k myfile
    atf_check -s exit:0 -o file:expout -e empty kyua list --kyuafile=myfile
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
sometest:pass
    test-suite = hello-world
sometest:skip
    test-suite = hello-world
EOF
    atf_expect_fail "Explicit test case did not trigger the processing of" \
        "the Kyuafile"
    atf_check -s exit:0 -o file:expout -e empty kyua list -k myfile sometest
    atf_check -s exit:0 -o file:expout -e empty kyua list --kyuafile=myfile \
        sometest
}


atf_init_test_cases() {
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

    atf_add_test_case kyuafile_flag__no_args
    atf_add_test_case kyuafile_flag__some_args

    atf_add_test_case verbose_flag

    atf_add_test_case missing_test_program
    atf_add_test_case missing_test_case

    atf_add_test_case missing_kyuafile__no_args
    atf_add_test_case missing_kyuafile__test_program
    atf_add_test_case missing_kyuafile__subdir

    atf_add_test_case bogus_kyuafile
    atf_add_test_case bogus_test_program
}
