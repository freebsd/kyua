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

/// \file utils/lua/test_utils.hpp
/// Utilities for tests of the lua modules.
///
/// This file is intended to be included once, and only once, for every test
/// program that needs it.  All the code is herein contained to simplify the
/// dependency chain in the build rules.

#if !defined(UTILS_LUA_TEST_UTILS_HPP)
#   define UTILS_LUA_TEST_UTILS_HPP
#else
#   error "utils/lua/test_utils.hpp can only be included once"
#endif

#include <atf-c++.hpp>

#include "utils/format/macros.hpp"
#include "utils/lua/exceptions.hpp"
#include "utils/lua/wrap.hpp"


namespace {


/// Checks that a given expression raises a particular lua::api_error.
///
/// We cannot make any assumptions regarding the error text provided by Lua, so
/// we resort to checking only which API function raised the error (because our
/// code is the one hardcoding these strings).
///
///
/// \param exp_api_function The name of the Lua C API function that causes the
///     error.
/// \param statement The statement to execute.
#define REQUIRE_API_ERROR(exp_api_function, statement) \
    do { \
        try { \
            statement; \
            ATF_FAIL("api_error not raised by " #statement); \
        } catch (const utils::lua::api_error& api_error) { \
            ATF_REQUIRE_EQ(exp_api_function, api_error.api_function()); \
        } \
    } while (0)


/// Gets the pointer to the internal lua_State of a state object.
///
/// \param state The Lua state.
///
/// \return The internal lua_State of the input Lua state.
inline lua_State*
raw(utils::lua::state& state)
{
    return reinterpret_cast< lua_State* >(state.raw_state_for_testing());
}


/// Ensures that the Lua stack maintains its original height upon exit.
///
/// Use an instance of this class to check that a piece of code does not have
/// side-effects on the Lua stack.
///
/// To be used within a test case only.
class stack_balance_checker {
    utils::lua::state& _state;
    bool _with_sentinel;
    unsigned int _old_count;

public:
    /// Constructs a new stack balance checker.
    ///
    /// \param state_ The Lua state to validate.
    /// \param with_sentinel_ If true, insert a sentinel item into the stack and
    ///     validate upon exit that the item is still there.  This is an attempt
    ///     to ensure that already-existing items are not removed from the stack
    ///     by the code under test.
    stack_balance_checker(utils::lua::state& state_,
                          const bool with_sentinel_ = true) :
        _state(state_),
        _with_sentinel(with_sentinel_),
        _old_count(_state.get_top())
    {
        if (_with_sentinel)
            _state.push_integer(987654321);
    }

    /// Destructor for the object.
    ///
    /// If the stack height does not match the height when the instance was
    /// created, this fails the test case.
    ~stack_balance_checker(void)
    {
        if (_with_sentinel) {
            if (!_state.is_number() || _state.to_integer() != 987654321)
                ATF_FAIL("Stack corrupted: sentinel not found");
            _state.pop(1);
        }

        unsigned int new_count = _state.get_top();
        if (_old_count != new_count)
            ATF_FAIL(F("Stack not balanced: before %d, after %d") %
                     _old_count % new_count);
    }
};


}  // anonymous namespace
