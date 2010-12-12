// Copyright 2010, Google Inc.
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

extern "C" {
#include <unistd.h>
}

#include <atf-c++.hpp>

#include "utils/passwd.hpp"

namespace passwd = utils::passwd;


ATF_TEST_CASE_WITHOUT_HEAD(user__is_root__true);
ATF_TEST_CASE_BODY(user__is_root__true)
{
    const passwd::user user(0);
    ATF_REQUIRE(user.is_root());
}


ATF_TEST_CASE_WITHOUT_HEAD(user__is_root__false);
ATF_TEST_CASE_BODY(user__is_root__false)
{
    const passwd::user user(123);
    ATF_REQUIRE(!user.is_root());
}


ATF_TEST_CASE_WITHOUT_HEAD(current_user);
ATF_TEST_CASE_BODY(current_user)
{
    const passwd::user user = passwd::current_user();
    ATF_REQUIRE_EQ(::getpid(), user.uid());
}


ATF_TEST_CASE_WITHOUT_HEAD(current_user__fake);
ATF_TEST_CASE_BODY(current_user__fake)
{
    const passwd::user new_user(::getpid() + 1);
    passwd::set_current_user_for_testing(new_user);

    const passwd::user user = passwd::current_user();
    ATF_REQUIRE(::getpid() != user.uid());
    ATF_REQUIRE_EQ(new_user.uid(), user.uid());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, user__is_root__true);
    ATF_ADD_TEST_CASE(tcs, user__is_root__false);

    ATF_ADD_TEST_CASE(tcs, current_user);
    ATF_ADD_TEST_CASE(tcs, current_user__fake);
}
