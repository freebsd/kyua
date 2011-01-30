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

#include "utils/format/macros.hpp"
#include "utils/lua/exceptions.hpp"
#include "utils/lua/operations.hpp"
#include "utils/lua/wrap.hpp"
#include "utils/sanity.hpp"

namespace fs = utils::fs;
namespace lua = utils::lua;


/// Creates a module: i.e. a table with a set of methods in it.
///
/// \param s The Lua state.
/// \param name The name of the module to create.
/// \param members The list of member functions to add to the module.
void
lua::create_module(state& s, const std::string& name,
                   const std::map< std::string, c_function >& members)
{
    stack_cleaner cleaner(s);
    s.new_table();
    for (std::map< std::string, c_function >::const_iterator
         iter = members.begin(); iter != members.end(); iter++) {
        s.push_string((*iter).first);
        s.push_c_function((*iter).second);
        s.set_table(-3);
    }
    s.set_global(name);
}


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
/// \throw lua::file_not_found_error If the file does not exist.
unsigned int
lua::do_file(state& s, const fs::path& file, const int nresults)
{
    PRE(nresults >= -1);
    const int height = s.get_top();

    stack_cleaner cleaner(s);
    try {
        s.load_file(file);
        s.pcall(0, nresults == -1 ? LUA_MULTRET : nresults, 0);
    } catch (const lua::file_not_found_error& e) {
        throw e;
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


/// Convenience function to evaluate a Lua expression.
///
/// \param s The Lua state.
/// \param expression The textual expression to evaluate.
/// \param nresults The number of results to leave on the stack.  Must be
///     positive.
///
/// \throw api_error If there is a problem evaluating the expression.
void
lua::eval(state& s, const std::string& expression, const int nresults)
{
    PRE(nresults > 0);
    do_string(s, F("return %s") % expression, nresults);
}
