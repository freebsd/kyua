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
#include "utils/lua/wrap.ipp"
#include "utils/noncopyable.hpp"
#include "utils/sanity.hpp"

namespace fs = utils::fs;
namespace lua = utils::lua;


namespace {


/// Wrapper around lua_getglobal to run in a protected environment.
///
/// \pre stack(-1) is the name of the global to get.
/// \post stack(-1) is the value of the global.
///
/// \param state The Lua C API state.
///
/// \return The number of return values pushed onto the stack.
static int
protected_getglobal(lua_State* state)
{
    lua_getglobal(state, lua_tostring(state, -1));
    return 1;
}


/// Wrapper around lua_gettable to run in a protected environment.
///
/// \pre stack(-2) is the table to get the element from.
/// \pre stack(-1) is the table index.
/// \post stack(-1) is the value of stack(-2)[stack(-1)].
///
/// \param state The Lua C API state.
///
/// \return The number of return values pushed onto the stack.
static int
protected_gettable(lua_State* state)
{
    lua_gettable(state, -2);
    return 1;
}


/// Wrapper around lua_setglobal to run in a protected environment.
///
/// \pre stack(-2) is the name of the global to set.
/// \pre stack(-1) is the value to set the global to.
///
/// \param state The Lua C API state.
///
/// \return The number of return values pushed onto the stack.
static int
protected_setglobal(lua_State* state)
{
    lua_setglobal(state, lua_tostring(state, -2));
    return 0;
}


/// Wrapper around lua_settable to run in a protected environment.
///
/// \pre stack(-3) is the table to set the element into.
/// \pre stack(-2) is the table index.
/// \pre stack(-1) is the value to set.
///
/// \param state The Lua C API state.
///
/// \return The number of return values pushed onto the stack.
static int
protected_settable(lua_State* state)
{
    lua_settable(state, -3);
    return 0;
}


}  // anonymous namespace


/// Internal implementation for lua::state.
struct utils::lua::state::impl {
    /// The Lua internal state.
    lua_State* lua_state;

    /// Whether we own the state or not (to decide if we close it).
    bool owned;

    /// Constructor.
    ///
    /// \param lua_ The Lua internal state.
    /// \param owned_ Whether we own the state or not.
    impl(lua_State* lua_, bool owned_) :
        lua_state(lua_),
        owned(owned_)
    {
    }
};


/// Initializes the Lua state.
///
/// You must share the same state object alongside the lifetime of your Lua
/// session.  As soon as the object is destroyed, the session is terminated.
lua::state::state(void)
{
    lua_State* lua = lua_open();
    if (lua == NULL)
        throw lua::error("lua open failed");
    _pimpl.reset(new impl(lua, true));
}


/// Initializes the Lua state from an existing raw state.
///
/// Instances constructed using this method do NOT own the raw state.  This
/// means that, on exit, the state will not be destroyed.
///
/// \param raw_state The raw Lua state to wrap.
lua::state::state(lua_State* raw_state) :
    _pimpl(new impl(raw_state, false))
{
}


/// Destructor for the Lua state.
///
/// Closes the session unless it has already been closed by calling the
/// close() method.  It is recommended to explicitly close the session in the
/// code.
lua::state::~state(void)
{
    if (_pimpl->owned && _pimpl->lua_state != NULL)
        close();
}


/// Terminates this Lua session.
///
/// It is recommended to call this instead of relying on the destructor to do
/// the cleanup, but it is not a requirement to use close().
///
/// \pre close() has not yet been called.
/// \pre The Lua stack is empty.  This is not truly necessary but ensures that
///     our code is consistent and clears the stack explicitly.
void
lua::state::close(void)
{
    PRE(_pimpl->lua_state != NULL);
    PRE(lua_gettop(_pimpl->lua_state) == 0);
    lua_close(_pimpl->lua_state);
    _pimpl->lua_state = NULL;
}


/// Wrapper around lua_getglobal.
///
/// \param name The second parameter to lua_getglobal.
///
/// \throw api_error If lua_getglobal fails.
///
/// \warning Terminates execution if there is not enough memory to manipulate
/// the Lua stack.
void
lua::state::get_global(const std::string& name)
{
    lua_pushcfunction(_pimpl->lua_state, protected_getglobal);
    lua_pushstring(_pimpl->lua_state, name.c_str());
    if (lua_pcall(_pimpl->lua_state, 1, 1, 0) != 0)
        throw lua::api_error::from_stack(_pimpl->lua_state, "lua_getglobal");
}


/// Wrapper around lua_gettable.
///
/// \param index The second parameter to lua_gettable.
///
/// \throw api_error If lua_gettable fails.
///
/// \warning Terminates execution if there is not enough memory to manipulate
/// the Lua stack.
void
lua::state::get_table(const int index)
{
    PRE(lua_gettop(_pimpl->lua_state) >= 2);
    lua_pushcfunction(_pimpl->lua_state, protected_gettable);
    lua_pushvalue(_pimpl->lua_state, index < 0 ? index - 1 : index);
    lua_pushvalue(_pimpl->lua_state, -3);
    if (lua_pcall(_pimpl->lua_state, 2, 1, 0) != 0)
        throw lua::api_error::from_stack(_pimpl->lua_state, "lua_gettable");
    lua_remove(_pimpl->lua_state, -2);
}


/// Wrapper around lua_gettop.
///
/// \return The return value of lua_gettop.
int
lua::state::get_top(void)
{
    return lua_gettop(_pimpl->lua_state);
}


/// Wrapper around lua_isboolean.
///
/// \param index The second parameter to lua_isboolean.
///
/// \return The return value of lua_isboolean.
bool
lua::state::is_boolean(const int index)
{
    return lua_isboolean(_pimpl->lua_state, index);
}


/// Wrapper around lua_isnil.
///
/// \param index The second parameter to lua_isnil.
///
/// \return The return value of lua_isnil.
bool
lua::state::is_nil(const int index)
{
    return lua_isnil(_pimpl->lua_state, index);
}


/// Wrapper around lua_isnumber.
///
/// \param index The second parameter to lua_isnumber.
///
/// \return The return value of lua_isnumber.
bool
lua::state::is_number(const int index)
{
    return lua_isnumber(_pimpl->lua_state, index);
}


/// Wrapper around lua_isstring.
///
/// \param index The second parameter to lua_isstring.
///
/// \return The return value of lua_isstring.
bool
lua::state::is_string(const int index)
{
    return lua_isstring(_pimpl->lua_state, index);
}


/// Wrapper around lua_istable.
///
/// \param index The second parameter to lua_istable.
///
/// \return The return value of lua_istable.
bool
lua::state::is_table(const int index)
{
    return lua_istable(_pimpl->lua_state, index);
}


/// Wrapper around luaL_loadfile.
///
/// \param file The second parameter to luaL_loadfile.
///
/// \throw api_error If luaL_loadfile returns an error.
///
/// \warning Terminates execution if there is not enough memory.
void
lua::state::load_file(const fs::path& file)
{
    if (luaL_loadfile(_pimpl->lua_state, file.c_str()) != 0)
        throw lua::api_error::from_stack(_pimpl->lua_state, "luaL_loadfile");
}


/// Wrapper around luaL_loadstring.
///
/// \param str The second parameter to the luaL_loadstring.
///
/// \throw api_error If luaL_loadstring returns an error.
///
/// \warning Terminates execution if there is not enough memory.
void
lua::state::load_string(const std::string& str)
{
    if (luaL_loadstring(_pimpl->lua_state, str.c_str()) != 0)
        throw lua::api_error::from_stack(_pimpl->lua_state, "luaL_loadstring");
}


/// Wrapper around lua_newtable.
///
/// \warning Terminates execution if there is not enough memory.
void
lua::state::new_table(void)
{
    lua_newtable(_pimpl->lua_state);
}


/// Wrapper around luaopen_base.
///
/// \throw api_error If luaopen_base fails.
///
/// \warning Terminates execution if there is not enough memory.
void
lua::state::open_base(void)
{
    lua_pushcfunction(_pimpl->lua_state, luaopen_base);
    if (lua_pcall(_pimpl->lua_state, 0, 0, 0) != 0)
        throw lua::api_error::from_stack(_pimpl->lua_state, "luaopen_base");
}


/// Wrapper around luaopen_string.
///
/// \throw api_error If luaopen_string fails.
///
/// \warning Terminates execution if there is not enough memory.
void
lua::state::open_string(void)
{
    lua_pushcfunction(_pimpl->lua_state, luaopen_string);
    if (lua_pcall(_pimpl->lua_state, 0, 0, 0) != 0)
        throw lua::api_error::from_stack(_pimpl->lua_state, "luaopen_string");
}


/// Wrapper around luaopen_table.
///
/// \throw api_error If luaopen_table fails.
///
/// \warning Terminates execution if there is not enough memory.
void
lua::state::open_table(void)
{
    lua_pushcfunction(_pimpl->lua_state, luaopen_table);
    if (lua_pcall(_pimpl->lua_state, 0, 0, 0) != 0)
        throw lua::api_error::from_stack(_pimpl->lua_state, "luaopen_table");
}


/// Wrapper around lua_pcall.
///
/// \param nargs The second parameter to lua_pcall.
/// \param nresults The third parameter to lua_pcall.
/// \param errfunc The fourth parameter to lua_pcall.
///
/// \throw api_error If lua_pcall returns an error.
void
lua::state::pcall(const int nargs, const int nresults, const int errfunc)
{
    if (lua_pcall(_pimpl->lua_state, nargs, nresults, errfunc) != 0)
        throw lua::api_error::from_stack(_pimpl->lua_state, "lua_pcall");
}


/// Wrapper around lua_pop.
///
/// \param count The second parameter to lua_pop.
void
lua::state::pop(const int count)
{
    PRE(count <= lua_gettop(_pimpl->lua_state));
    lua_pop(_pimpl->lua_state, count);
    POST(lua_gettop(_pimpl->lua_state) >= 0);
}


/// Wrapper around lua_pushcfunction.
///
/// \param function The second parameter to lua_pushcfuntion.  Use the
///     wrap_cxx_function wrapper to provide C++ functions to this parameter.
///
/// \warning Terminates execution if there is not enough memory.
void
lua::state::push_c_function(c_function function)
{
    lua_pushcfunction(_pimpl->lua_state, function);
}


/// Wrapper around lua_pushinteger.
///
/// \param value The second parameter to lua_pushinteger.
void
lua::state::push_integer(const int value)
{
    lua_pushinteger(_pimpl->lua_state, value);
}


/// Wrapper around lua_pushstring.
///
/// \param str The second parameter to lua_pushcfuntion.
///
/// \warning Terminates execution if there is not enough memory.
void
lua::state::push_string(const std::string& str)
{
    lua_pushstring(_pimpl->lua_state, str.c_str());
}


/// Wrapper around lua_setglobal.
///
/// \param name The second parameter to lua_setglobal.
///
/// \throw api_error If lua_setglobal fails.
///
/// \warning Terminates execution if there is not enough memory to manipulate
/// the Lua stack.
void
lua::state::set_global(const std::string& name)
{
    lua_pushcfunction(_pimpl->lua_state, protected_setglobal);
    lua_pushstring(_pimpl->lua_state, name.c_str());
    lua_pushvalue(_pimpl->lua_state, -3);
    if (lua_pcall(_pimpl->lua_state, 2, 0, 0) != 0)
        throw lua::api_error::from_stack(_pimpl->lua_state, "lua_setglobal");
    lua_pop(_pimpl->lua_state, 1);
}


/// Wrapper around lua_settable.
///
/// \param index The second parameter to lua_settable.
///
/// \throw api_error If lua_settable fails.
///
/// \warning Terminates execution if there is not enough memory to manipulate
/// the Lua stack.
void
lua::state::set_table(const int index)
{
    lua_pushcfunction(_pimpl->lua_state, protected_settable);
    lua_pushvalue(_pimpl->lua_state, index < 0 ? index - 1 : index);
    lua_pushvalue(_pimpl->lua_state, -4);
    lua_pushvalue(_pimpl->lua_state, -4);
    if (lua_pcall(_pimpl->lua_state, 3, 0, 0) != 0)
        throw lua::api_error::from_stack(_pimpl->lua_state, "lua_settable");
    lua_pop(_pimpl->lua_state, 2);
}


/// Wrapper around lua_toboolean.
///
/// \param index The second parameter to lua_toboolean.
///
/// \return The return value of lua_toboolean.
bool
lua::state::to_boolean(const int index)
{
    PRE(is_boolean(index));
    return lua_toboolean(_pimpl->lua_state, index);
}


/// Wrapper around lua_tointeger.
///
/// \param index The second parameter to lua_tointeger.
///
/// \return The return value of lua_tointeger.
long
lua::state::to_integer(const int index)
{
    PRE(is_number(index));
    return lua_tointeger(_pimpl->lua_state, index);
}


/// Wrapper around lua_tostring.
///
/// \param index The second parameter to lua_tostring.
///
/// \return The return value of lua_tostring.
///
/// \warning Terminates execution if there is not enough memory.
std::string
lua::state::to_string(const int index)
{
    PRE(is_string(index));
    const char *raw_string = lua_tostring(_pimpl->lua_state, index);
    // Note that the creation of a string object below (explicit for clarity)
    // implies that the raw string is duplicated and, henceforth, the string is
    // safe even if the corresponding element is popped from the Lua stack.
    return std::string(raw_string);
}


/// Gets the internal lua_State object for testing purposes only.
///
/// \return The raw Lua state.
lua_State*
lua::state::raw_state_for_testing(void)
{
    return _pimpl->lua_state;
}


/// Internal implementation for lua::stack_cleaner.
struct utils::lua::stack_cleaner::impl {
    /// Reference to the Lua state this stack_cleaner refers to.
    state& state_ref;

    /// The depth of the Lua stack to be restored.
    unsigned int original_depth;

    /// Constructor.
    ///
    /// \param state_ref_ Reference to the Lua state.
    /// \param original_depth_ The depth of the Lua stack.
    impl(state& state_ref_, const unsigned int original_depth_) :
        state_ref(state_ref_),
        original_depth(original_depth_)
    {
    }
};


/// Creates a new stack cleaner.
///
/// This gathers the current height of the stack so that extra elements can be
/// popped during destruction.
///
/// \param state_ The Lua state.
lua::stack_cleaner::stack_cleaner(state& state_) :
    _pimpl(new impl(state_, state_.get_top()))
{
}


/// Pops any values from the stack not known at construction time.
///
/// \pre The current height of the stack must be equal or greater to the height
/// of the stack when this object was instantiated.
lua::stack_cleaner::~stack_cleaner(void)
{
    const unsigned int current_depth = _pimpl->state_ref.get_top();
    PRE_MSG(current_depth >= _pimpl->original_depth,
            F("Unbalanced scope: current stack depth %d < original %d") %
            current_depth % _pimpl->original_depth);
    const unsigned int diff = current_depth - _pimpl->original_depth;
    if (diff > 0)
        _pimpl->state_ref.pop(diff);
}


/// Forgets about any elements currently in the stack.
///
/// This allows a function to return values on the stack because all the
/// elements that are currently in the stack will not be touched during
/// destruction when the function is called.
void
lua::stack_cleaner::forget(void)
{
    _pimpl->original_depth = _pimpl->state_ref.get_top();
}


/// Calls a C++ Lua function from a C calling environment.
///
/// Any errors reported by the C++ function are caught and reported to the
/// caller as Lua errors.
///
/// \param function The C++ function to call.
/// \param raw_state The raw Lua state.
///
/// \return The number of return values pushed onto the Lua stack by the
/// function.
int
utils::lua::detail::call_cxx_function_from_c(cxx_function function,
                                             lua_State* raw_state) throw()
{
    try {
        lua::state state(raw_state);
        return function(state);
    } catch (const std::exception& e) {
        return luaL_error(raw_state, "%s", e.what());
    } catch (...) {
        return luaL_error(raw_state, "Unhandled exception in Lua C++ hook");
    }
}
