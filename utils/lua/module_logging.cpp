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

#include <stdexcept>

#include "utils/logging/operations.hpp"
#include "utils/lua/module_logging.hpp"
#include "utils/lua/operations.hpp"
#include "utils/lua/wrap.ipp"

namespace logging = utils::logging;
namespace lua = utils::lua;


namespace {


/// Helper function for the other logging functions.
///
/// \pre stack(-1) The message to log.
///
/// \param level The level of the message to log.
/// \param state The Lua state.
///
/// \return The number of result values, i.e. 0.
///
/// \throw std::runtime_error If the parameters to the function are invalid.
static int
do_logging(const logging::level& level, lua::state& state)
{
    if (!state.is_string(-1))
      throw std::runtime_error("The logging message must be a string");
    const std::string message(state.to_string(-1));

    lua::debug ar;
    state.get_stack(1, &ar);
    state.get_info("Sl", &ar);

    logging::log(level, ar.source, ar.currentline, message);

    return 0;
}


/// Lua binding for logging::error.
///
/// \pre stack(-1) The message to log.
///
/// \param state The Lua state.
///
/// \return The number of result values, i.e. 0.
int
lua_logging_error(lua::state& state)
{
    return do_logging(logging::level_error, state);
}


/// Lua binding for logging::warning.
///
/// \pre stack(-1) The message to log.
///
/// \param state The Lua state.
///
/// \return The number of result values, i.e. 0.
int
lua_logging_warning(lua::state& state)
{
    return do_logging(logging::level_warning, state);
}


/// Lua binding for logging::info.
///
/// \pre stack(-1) The message to log.
///
/// \param state The Lua state.
///
/// \return The number of result values, i.e. 0.
int
lua_logging_info(lua::state& state)
{
    return do_logging(logging::level_info, state);
}


/// Lua binding for logging::debug.
///
/// \pre stack(-1) The message to log.
///
/// \param state The Lua state.
///
/// \return The number of result values, i.e. 0.
int
lua_logging_debug(lua::state& state)
{
    return do_logging(logging::level_debug, state);
}


}  // anonymous namespace


/// Creates a Lua 'logging' module.
///
/// \post The global 'logging' symbol is set to a table that contains functions
/// to a variety of utilites from the logging C++ module.
///
/// \param s The Lua state.
void
lua::open_logging(lua::state& s)
{
    std::map< std::string, lua::c_function > members;
    members["error"] = wrap_cxx_function< lua_logging_error >;
    members["warning"] = wrap_cxx_function< lua_logging_warning >;
    members["info"] = wrap_cxx_function< lua_logging_info >;
    members["debug"] = wrap_cxx_function< lua_logging_debug >;
    lua::create_module(s, "logging", members);
}
