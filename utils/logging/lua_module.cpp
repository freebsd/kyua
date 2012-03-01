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

#include "utils/logging/lua_module.hpp"

#include <stdexcept>

#include <lutok/debug.hpp>
#include <lutok/operations.hpp>
#include <lutok/state.ipp>

#include "utils/logging/operations.hpp"

namespace logging = utils::logging;


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
do_logging(const logging::level& level, lutok::state& state)
{
    if (!state.is_string(-1))
      throw std::runtime_error("The logging message must be a string");
    const std::string message(state.to_string(-1));

    lutok::debug debug;
    debug.get_stack(state, 1);
    debug.get_info(state, "Sl");

    logging::log(level, debug.source().c_str(), debug.current_line(), message);

    return 0;
}


/// Lua binding for logging::error.
///
/// \pre stack(-1) The message to log.
///
/// \param state The Lua state.
///
/// \return The number of result values, i.e. 0.
static int
lua_logging_error(lutok::state& state)
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
static int
lua_logging_warning(lutok::state& state)
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
static int
lua_logging_info(lutok::state& state)
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
static int
lua_logging_debug(lutok::state& state)
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
logging::open_logging(lutok::state& s)
{
    std::map< std::string, lutok::cxx_function > members;
    members["error"] = lua_logging_error;
    members["warning"] = lua_logging_warning;
    members["info"] = lua_logging_info;
    members["debug"] = lua_logging_debug;
    lutok::create_module(s, "logging", members);
}
