// Copyright 2012 Google Inc.
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

#include "utils/config/parser.hpp"

#include <lutok/exceptions.hpp>
#include <lutok/operations.hpp>
#include <lutok/stack_cleaner.hpp>
#include <lutok/state.ipp>

#include "utils/config/exceptions.hpp"
#include "utils/config/lua_module.hpp"
#include "utils/config/tree.ipp"
#include "utils/noncopyable.hpp"

namespace config = utils::config;


/// Internal implementation of the parser.
struct utils::config::parser::impl : utils::noncopyable {
    /// Pointer to the parent parser.  Needed for callbacks.
    parser* _parent;

    /// The Lua state used by this parser to process the configuration file.
    lutok::state _state;

    /// The tree to be filed in by the configuration parameters, as provided by
    /// the caller.
    config::tree& _tree;

    /// Constructs a new implementation.
    ///
    /// \param parent_ Pointer to the class being constructed.
    /// \param config_tree_ The configuration tree provided by the user.
    impl(parser* const parent_, tree& config_tree_) :
        _parent(parent_), _tree(config_tree_)
    {
    }

    friend void lua_syntax(lutok::state&);

    /// Callback executed by the Lua syntax() function.
    ///
    /// \param syntax_format The syntax format name as provided by the
    ///     configuration file in the call to syntax().
    /// \param syntax_version The syntax format version as provided by the
    ///     configuration file in the call to syntax().
    void
    syntax_callback(const std::string& syntax_format, const int syntax_version)
    {
        // Allow the parser caller to populate the tree with its own schema
        // depending on the format/version combination.
        _parent->setup(_tree, syntax_format, syntax_version);

        // Export the config module to the Lua state so that all global variable
        // accesses are redirected to the configuration tree.
        config::redirect(_state, _tree);
    }
};


namespace {


/// Implementation of the Lua syntax() function.
///
/// The syntax() function has to be called by configuration files as the very
/// first thing they do.  Once called, this function populates the configuration
/// tree based on the syntax format/version combination and then continues to
/// process the rest of the file.
///
/// \pre state(-2) The syntax format name.
/// \pre state(-1) The syntax format version.
///
/// \param state The Lua state to operate in.
///
/// \return The number of results pushed onto the stack; always 0.
static int
lua_syntax(lutok::state& state)
{
    if (!state.is_string(-2))
        throw config::value_error("First argument to syntax must be a string");
    const std::string syntax_format = state.to_string(-2);

    if (!state.is_number(-1))
        throw config::value_error("Second argument to syntax must be a number");
    const int syntax_version = state.to_integer(-1);

    state.get_global("_syntax_called");
    if (!state.is_nil())
        throw config::value_error("syntax() can only be invoked once");
    state.push_boolean(true);
    state.set_global("_syntax_called");
    state.pop(1);

    state.get_global("_config_parser");
    config::parser::impl* impl = *state.to_userdata< config::parser::impl* >();
    state.pop(1);

    impl->syntax_callback(syntax_format, syntax_version);

    return 0;
}


}  // anonymous namespace


/// Constructs a new parser.
///
/// \param [in,out] config_tree The configuration tree into which the values set
///     in the configuration file will be stored.
config::parser::parser(tree& config_tree) :
    _pimpl(new impl(this, config_tree))
{
    lutok::stack_cleaner cleaner(_pimpl->_state);

    _pimpl->_state.push_cxx_function(lua_syntax);
    _pimpl->_state.set_global("syntax");
    *_pimpl->_state.new_userdata< config::parser::impl* >() = _pimpl.get();
    _pimpl->_state.set_global("_config_parser");
}


/// Destructor.
config::parser::~parser(void)
{
}


/// Parses a configuration file.
///
/// \post The tree registered during the construction of this class is updated
/// to contain the values read from the configuration file.  If the processing
/// fails, the state of the output tree is undefined.
///
/// \param file The path to the file to process.
///
/// \throw syntax_error If there is any problem processing the file.
void
config::parser::parse(const fs::path& file)
{
    try {
        lutok::do_file(_pimpl->_state, file.str());
    } catch (const lutok::error& e) {
        throw syntax_error(e.what());
    }
}
