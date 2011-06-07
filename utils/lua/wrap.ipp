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

#if !defined(UTILS_LUA_WRAP_IPP)
#define UTILS_LUA_WRAP_IPP

#include <lua.hpp>

#include "utils/lua/wrap.hpp"

namespace utils {
namespace lua {


/// The type of a C++ function that can be bound into Lua.
///
/// To pass such a function to Lua, convert it to a C function with the
/// wrap_cxx_function template and pass it to, e.g. state::push_cfunction.
///
/// Functions of this type are free to raise exceptions.  These will not
/// propagate into the Lua C API.
typedef int (*cxx_function)(lua::state&);


namespace detail {
int call_cxx_function_from_c(cxx_function, lua_State*) throw();
}  // namespace detail


/// Wraps a C++ Lua function into a C function.
///
/// You can pass the generated function to, e.g. state::push_cfunction.
/// This wrapper ensures that exceptions do not propagate out of the C++ world
/// into the C realm.  Exceptions are reported as Lua errors to the caller.
///
/// \param raw_state The raw Lua state.
///
/// \return The number of return values pushed onto the Lua stack by the
/// function.
///
/// \warning Due to C++ standard and/or compiler oddities, functions passed to
/// this template must have external linkage.  In other words, static methods
/// cannot be passed to this if you want your code to build.
template< cxx_function Function >
int
wrap_cxx_function(lua_State* raw_state)
{
    return detail::call_cxx_function_from_c(Function, raw_state);
}


/// Wrapper around lua_newuserdata.
///
/// This allocates an object as big as the size of the provided Type.
///
/// \return The pointer to the allocated userdata object.
///
/// \warning Terminates execution if there is not enough memory.
template< typename Type >
Type*
state::new_userdata(void)
{
    return static_cast< Type* >(new_userdata_voidp(sizeof(Type)));
}


/// Wrapper around lua_touserdata.
///
/// \param index The second parameter to lua_touserdata.
///
/// \return The return value of lua_touserdata.
template< typename Type >
Type*
state::to_userdata(const int index)
{
    return static_cast< Type* >(to_userdata_voidp(index));
}


}  // namespace lua
}  // namespace utils

#endif  // !defined(UTILS_LUA_WRAP_IPP)
