// Copyright 2011, Google Inc.
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

#include "utils/format/macros.hpp"
#include "utils/lua/exceptions.hpp"
#include "utils/lua/operations.hpp"
#include "utils/lua/wrap.hpp"
#include "utils/sanity.hpp"

namespace fs = utils::fs;
namespace lua = utils::lua;


/// Loads and processes a Lua file.
///
/// This is a replacement for luaL_dofile but with proper error reporting
/// and stack control.
///
/// \param s The Lua state.
/// \param file The file to load.
/// \param nresults The number of results to expect; -1 for any.
///
/// \return The number of results left on the stack.
///
/// \throw lua::error If there is a problem processing the file.
unsigned int
lua::do_file(state& s, const fs::path& file, const int nresults)
{
    PRE(nresults >= -1);
    const int height = s.get_top();

    stack_cleaner cleaner(s);
    try {
        s.load_file(file);
        s.pcall(0, nresults == -1 ? LUA_MULTRET : nresults, 0);
    } catch (const lua::api_error& e) {
        throw lua::error(F("Failed to load Lua file '%s': %s") % file %
                         e.what());
    }
    cleaner.forget();

    const int actual_results = s.get_top() - height;
    INV(nresults == -1 || actual_results == nresults);
    INV(actual_results >= 0);
    return static_cast< unsigned int >(actual_results);
}


/// Processes a Lua script.
///
/// This is a replacement for luaL_dostring but with proper error reporting
/// and stack control.
///
/// \param s The Lua state.
/// \param str The string to process.
/// \param nresults The number of results to expect; -1 for any.
///
/// \return The number of results left on the stack.
///
/// \throw lua::error If there is a problem processing the string.
unsigned int
lua::do_string(state& s, const std::string& str, const int nresults)
{
    PRE(nresults >= -1);
    const int height = s.get_top();

    stack_cleaner cleaner(s);
    try {
        s.load_string(str);
        s.pcall(0, nresults == -1 ? LUA_MULTRET : nresults, 0);
    } catch (const lua::api_error& e) {
        throw lua::error(F("Failed to process Lua string '%s': %s") % str
                         % e.what());
    }
    cleaner.forget();

    const int actual_results = s.get_top() - height;
    INV(nresults == -1 || actual_results == nresults);
    INV(actual_results >= 0);
    return static_cast< unsigned int >(actual_results);
}


/// Queries and returns an array of strings.
///
/// \param s The Lua state.
/// \param name_expr An expression that yields the name of the array to get.
///
/// \return The elements of the array.
///
/// \throw api_error If the name_expr has a syntax error or fails to be
///     evaluated.
/// \throw error If any other error happens.
std::vector< std::string >
lua::get_array_as_strings(state& s, const std::string& name_expr)
{
    stack_cleaner cleaner1(s);

    s.load_string(F("return (%s);") % name_expr);
    s.pcall(0, 1, 0);
    if (s.is_nil())
        throw error(F("Undefined array '%s'") % name_expr);
    if (!s.is_table())
        throw error(F("'%s' not an array") % name_expr);

    std::vector< std::string > array;
    int index = 1;
    do {
        stack_cleaner cleaner2(s);

        s.push_integer(index);
        s.get_table();
        if (s.is_nil())
            index = -1;
        else {
            if (!s.is_string())
                throw error(F("Invalid non-string value in array '%s'") %
                            name_expr);
            array.push_back(s.to_string());
            index++;
        }
    } while (index > 0);
    return array;
}
