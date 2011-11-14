// Copyright 2011 Google Inc.
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

#include <map>
#include <string>

#include <atf-c++.hpp>

#include "engine/action.hpp"
#include "engine/context.hpp"
#include "utils/fs/path.hpp"

namespace fs = utils::fs;


namespace {


/// Generates a context with fake data for testing purposes only.
///
/// \return The fake context.
static engine::context
fake_context(const char* cwd = "/foo/bar")
{
    std::map< std::string, std::string > env;
    env["foo"] = "bar";
    return engine::context(fs::path(cwd), env);
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(ctor_and_getters);
ATF_TEST_CASE_BODY(ctor_and_getters)
{
    const engine::context context = fake_context();
    const engine::action action(context);
    ATF_REQUIRE_EQ(context.unique_address(),
                   action.runtime_context().unique_address());
}


ATF_TEST_CASE_WITHOUT_HEAD(unique_address);
ATF_TEST_CASE_BODY(unique_address)
{
    const engine::context context = fake_context();
    const engine::action action1(context);
    {
        const engine::action action2 = action1;
        const engine::action action3(context);
        ATF_REQUIRE(action1.unique_address() == action2.unique_address());
        ATF_REQUIRE(action1.unique_address() != action3.unique_address());
        ATF_REQUIRE(action2.unique_address() != action3.unique_address());
    }
    ATF_REQUIRE(action1.unique_address() == action1.unique_address());
}


ATF_TEST_CASE_WITHOUT_HEAD(operator_eq);
ATF_TEST_CASE_BODY(operator_eq)
{
    const engine::action action1(fake_context("foo/bar"));
    const engine::action action2(fake_context("foo/bar"));
    const engine::action action3(fake_context("foo/baz"));
    ATF_REQUIRE(  action1 == action2);
    ATF_REQUIRE(!(action1 == action3));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, ctor_and_getters);
    ATF_ADD_TEST_CASE(tcs, unique_address);
    ATF_ADD_TEST_CASE(tcs, operator_eq);
}
