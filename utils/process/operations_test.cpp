// Copyright 2014 Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "utils/process/operations.hpp"

#include <cerrno>

#include <atf-c++.hpp>

#include "utils/format/containers.ipp"
#include "utils/process/child.ipp"
#include "utils/process/exceptions.hpp"

namespace process = utils::process;


namespace {


/// Body for a process that returns a specific exit code.
///
/// \tparam ExitStatus The exit status for the subprocess.
template< int ExitStatus >
static void
child_exit(void)
{
    std::exit(ExitStatus);
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(wait_any__one);
ATF_TEST_CASE_BODY(wait_any__one)
{
    process::child::fork_capture(child_exit< 15 >);

    const process::status status = process::wait_any();
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(15, status.exitstatus());
}


ATF_TEST_CASE_WITHOUT_HEAD(wait_any__many);
ATF_TEST_CASE_BODY(wait_any__many)
{
    process::child::fork_capture(child_exit< 15 >);
    process::child::fork_capture(child_exit< 30 >);
    process::child::fork_capture(child_exit< 45 >);

    std::set< int > exit_codes;
    for (int i = 0; i < 3; i++) {
        const process::status status = process::wait_any();
        ATF_REQUIRE(status.exited());
        exit_codes.insert(status.exitstatus());
    }

    std::set< int > exp_exit_codes;
    exp_exit_codes.insert(15);
    exp_exit_codes.insert(30);
    exp_exit_codes.insert(45);
    ATF_REQUIRE_EQ(exp_exit_codes, exit_codes);
}


ATF_TEST_CASE_WITHOUT_HEAD(wait_any__none_is_failure);
ATF_TEST_CASE_BODY(wait_any__none_is_failure)
{
    try {
        const process::status status = process::wait_any();
        fail("Expected exception but none raised");
    } catch (const process::system_error& e) {
        ATF_REQUIRE(atf::utils::grep_string("Failed to wait", e.what()));
        ATF_REQUIRE_EQ(ECHILD, e.original_errno());
    }
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, wait_any__one);
    ATF_ADD_TEST_CASE(tcs, wait_any__many);
    ATF_ADD_TEST_CASE(tcs, wait_any__none_is_failure);
}
