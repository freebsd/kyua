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

#include "utils/releaser.hpp"

#include <cstddef>
#include <string>

#include <atf-c++.hpp>


namespace {


/// Number of times free_hook has been caller.
static ssize_t free_calls = 0;


/// Deletes the given object of integer type for testing purposes.
///
/// \tparam T The type of the object to be released.
/// \param value The actual object to be released.
template< class T >
void
free_hook(T* value)
{
    delete value;
    ++free_calls;
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(scope);
ATF_TEST_CASE_BODY(scope)
{
    {
        int* i = new int(5);
        ATF_REQUIRE_EQ(::free_calls, 0);
        utils::releaser< int, void > releaser(i, ::free_hook< int >);
        ATF_REQUIRE_EQ(::free_calls, 0);
    }
    ATF_REQUIRE_EQ(::free_calls, 1);
    {
        std::string* s = new std::string("foo bar");
        ATF_REQUIRE_EQ(::free_calls, 1);
        utils::releaser< std::string, void > releaser(
            s, ::free_hook< std::string >);
        ATF_REQUIRE_EQ(::free_calls, 1);
    }
    ATF_REQUIRE_EQ(::free_calls, 2);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, scope);
}
