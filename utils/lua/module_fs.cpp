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

#include "utils/lua/module_fs.hpp"
#include "utils/lua/operations.hpp"
#include "utils/lua/wrap.ipp"

namespace fs = utils::fs;
namespace lua = utils::lua;


namespace {


/// Lua binding for fs::path::basename.
///
/// \pre stack(-1) The input path.
/// \post stack(-1) The basename of the input path.
///
/// \param state The Lua state.
///
/// \return The number of result values, i.e. 1.
int
lua_fs_basename(lua::state& state)
{
    const fs::path path(state.to_string());
    state.push_string(path.leaf_name().c_str());
    return 1;
}


/// Lua binding for fs::path::dirname.
///
/// \pre stack(-1) The input path.
/// \post stack(-1) The directory part of the input path.
///
/// \param state The Lua state.
///
/// \return The number of result values, i.e. 1.
int
lua_fs_dirname(lua::state& state)
{
    const fs::path path(state.to_string());
    state.push_string(path.branch_path().c_str());
    return 1;
}


/// Lua binding for fs::path::operator/.
///
/// \pre stack(-2) The first input path.
/// \pre stack(-1) The second input path.
/// \post stack(-1) The concatenation of the two paths.
///
/// \param state The Lua state.
///
/// \return The number of result values, i.e. 1.
int
lua_fs_join(lua::state& state)
{
    const fs::path path1(state.to_string(-2));
    const fs::path path2(state.to_string(-1));
    state.push_string((path1 / path2).c_str());
    return 1;
}


}  // anonymous namespace


/// Creates a Lua 'fs' module.
///
/// \post The global 'fs' symbol is set to a table that contains functions to a
/// variety of utilites from the fs C++ module.
///
/// \param s The Lua state.
void
lua::open_fs(lua::state& s)
{
    std::map< std::string, lua::c_function > members;
    members["basename"] = wrap_cxx_function< lua_fs_basename >;
    members["dirname"] = wrap_cxx_function< lua_fs_dirname >;
    members["join"] = wrap_cxx_function< lua_fs_join >;
    lua::create_module(s, "fs", members);
}
