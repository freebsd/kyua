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

#include <lua.hpp>

#include "utils/format/macros.hpp"
#include "utils/lua/exceptions.hpp"
#include "utils/lua/wrap.hpp"
#include "utils/sanity.hpp"

namespace lua = utils::lua;


/// Constructs a new error with a plain-text message.
///
/// \param message The plain-text error message.
lua::error::error(const std::string& message) :
    std::runtime_error(message)
{
}


/// Destructor for the error.
lua::error::~error(void) throw()
{
}


/// Constructs a new error.
///
/// \param api_function_ The name of the API function that caused the error.
/// \param message The plain-text error message provided by Lua.
lua::api_error::api_error(const std::string& api_function_,
                          const std::string& message) :
    error(message),
    _api_function(api_function_)
{
}


/// Destructor for the error.
lua::api_error::~api_error(void) throw()
{
}


/// Constructs a new api_error with the message on the top of the Lua stack.
///
/// \pre There is an error message on the top of the stack.
/// \post The error message is popped from the stack.
///
/// \param s The Lua state.
/// \param api_function_ The name of the Lua API function that caused the error.
///
/// \return A new api_error with the popped message.
lua::api_error
lua::api_error::from_stack(lua_State* s, const std::string& api_function_)
{
    PRE(lua_isstring(s, -1));
    const std::string message = lua_tostring(s, -1);
    lua_pop(s, 1);
    return lua::api_error(api_function_, message);
}


/// Gets the name of the Lua API function that caused this error.
///
/// \return The name of the function.
const std::string&
lua::api_error::api_function(void) const
{
    return _api_function;
}
