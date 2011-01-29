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

#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include <atf-c++.hpp>
#include <lua.hpp>

#include "utils/format/macros.hpp"
#include "utils/lua/exceptions.hpp"
#include "utils/lua/test_utils.hpp"
#include "utils/lua/wrap.ipp"

namespace fs = utils::fs;
namespace lua = utils::lua;


// A note about the lua::state tests.
//
// The methods of lua::state are, in general, thin wrappers around the
// corresponding Lua C API methods.  The tests below are simple unit tests that
// ensure that these functions just delegate the calls to the Lua library.  We
// do not intend to test the validity of the methods themselves (that's the
// job of the Lua authors).  That said, we test those conditions we rely on,
// such as the reporting of errors and the default values to the API.
//
// Lastly, for every test case that stresses a single lua::state method, we only
// call that method directly.  All other Lua state manipulation operations are
// performed by means of direct calls to the Lua C API.  This is to ensure that
// the wrapped methods are really talking to Lua.


namespace {


/// Checks if a symbol is available.
///
/// \param state The Lua state.
/// \param symbol The symbol to check for.
///
/// \return True if the symbol is defined, false otherwise.
static bool
is_available(lua::state& state, const char* symbol)
{
    luaL_loadstring(raw(state), (F("return %s") % symbol).str().c_str());
    const bool ok = (lua_pcall(raw(state), 0, 1, 0) == 0 &&
                     !lua_isnil(raw(state), -1));
    lua_pop(raw(state), 1);
    std::cout << "Symbol " << symbol << (ok ? " found\n" : " not found\n");
    return ok;
}


/// Checks that no modules are present or that only one has been loaded.
///
/// \post The test case terminates if there is any module present when expected
/// is empty or if there two modules loaded when expected is defined.
///
/// \param state The Lua state.
/// \param expected The module to expect.  Empty if no modules are allowed.
static void
check_modules(lua::state& state, const std::string& expected)
{
    std::cout << "Checking loaded modules" <<
        (expected.empty() ? "" : (" (" + expected + " expected)")) << "\n";
    ATF_REQUIRE(!((expected == "base") ^ (is_available(state, "assert"))));
    ATF_REQUIRE(!((expected == "string") ^
                  (is_available(state, "string.byte"))));
    ATF_REQUIRE(!((expected == "table") ^
                  (is_available(state, "table.concat"))));
}


/// A custom C multiply function for Lua.
///
/// \pre stack(-2) contains the first factor.
/// \pre stack(-1) contains the second factor.
/// \post stack(-1) contains the product of the two input factors.
///
/// \param state The raw Lua state.
///
/// \return The number of result values, i.e. 1.
static int
c_multiply(lua_State* state)
{
    const int f1 = lua_tointeger(state, -2);
    const int f2 = lua_tointeger(state, -1);
    lua_pushinteger(state, f1 * f2);
    return 1;
}


/// A custom C++ integral division function for Lua.
///
/// \pre stack(-2) contains the dividend.
/// \pre stack(-1) contains the divisor.
/// \post stack(-2) contains the quotient of the division.
/// \post stack(-1) contains the remainder of the division.
///
/// \param state The Lua state.
///
/// \return The number of result values, i.e. 1.
///
/// \throw std::runtime_error If the divisor is zero.
/// \throw std::string If the dividend or the divisor are negative.  This is an
///     exception not derived from std::exception on purpose to ensure that the
///     C++ wrapping correctly captures any exception regardless of its type.
int  // Not static because it needs external linkage for wrap_cxx_function.
cxx_divide(lua::state& state)
{
    const int dividend = state.to_integer(-2);
    const int divisor = state.to_integer(-1);
    if (divisor == 0)
        throw std::runtime_error("Divisor is 0");
    if (dividend < 0 || divisor < 0)
        throw std::string("Cannot divide negative numbers");
    state.push_integer(dividend / divisor);
    state.push_integer(dividend % divisor);
    return 2;
}


/// A Lua function that raises a very long error message.
///
/// \pre stack(-1) contains the length of the message to construct.
///
/// \param state The Lua state.
///
/// \return Never returns.
///
/// \throw std::runtime_error Unconditionally, with an error message formed by
///     the repetition of 'A' as many times as requested.
int  // Not static because it needs external linkage for wrap_cxx_function.
raise_long_error(lua::state& state)
{
    const int length = state.to_integer();
    throw std::runtime_error(std::string(length, 'A').c_str());
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(state__ctor_only_wrap);
ATF_TEST_CASE_BODY(state__ctor_only_wrap)
{
    lua_State* raw_state = lua_open();
    ATF_REQUIRE(raw_state != NULL);

    {
        lua::state state(raw_state);
        lua_pushinteger(raw(state), 123);
    }
    // If the wrapper object had closed the Lua state, we could very well crash
    // here.
    ATF_REQUIRE_EQ(123, lua_tointeger(raw_state, -1));

    lua_close(raw_state);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__close);
ATF_TEST_CASE_BODY(state__close)
{
    lua::state state;
    state.close();
    // The destructor for state will run now.  If it does a second close, we may
    // crash, so let's see if we don't.
}


ATF_TEST_CASE_WITHOUT_HEAD(state__get_global__ok);
ATF_TEST_CASE_BODY(state__get_global__ok)
{
    lua::state state;
    ATF_REQUIRE(luaL_dostring(raw(state), "test_variable = 3") == 0);
    state.get_global("test_variable");
    ATF_REQUIRE(lua_isnumber(raw(state), -1));
    lua_pop(raw(state), 1);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__get_global__fail);
ATF_TEST_CASE_BODY(state__get_global__fail)
{
    lua::state state;
    lua_pushinteger(raw(state), 3);
    lua_replace(raw(state), LUA_GLOBALSINDEX);
    REQUIRE_API_ERROR("lua_getglobal", state.get_global("test_variable"));
}


ATF_TEST_CASE_WITHOUT_HEAD(state__get_global__undefined);
ATF_TEST_CASE_BODY(state__get_global__undefined)
{
    lua::state state;
    state.get_global("test_variable");
    ATF_REQUIRE(lua_isnil(raw(state), -1));
    lua_pop(raw(state), 1);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__get_table__ok);
ATF_TEST_CASE_BODY(state__get_table__ok)
{
    lua::state state;
    ATF_REQUIRE(luaL_dostring(raw(state), "t = { a = 1, bar = 234 }") == 0);
    lua_getglobal(raw(state), "t");
    lua_pushstring(raw(state), "bar");
    state.get_table();
    ATF_REQUIRE(lua_isnumber(raw(state), -1));
    ATF_REQUIRE_EQ(234, lua_tointeger(raw(state), -1));
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__get_table__nil);
ATF_TEST_CASE_BODY(state__get_table__nil)
{
    lua::state state;
    lua_pushnil(raw(state));
    lua_pushinteger(raw(state), 1);
    REQUIRE_API_ERROR("lua_gettable", state.get_table());
    ATF_REQUIRE_EQ(2, lua_gettop(raw(state)));
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__get_table__unknown_index);
ATF_TEST_CASE_BODY(state__get_table__unknown_index)
{
    lua::state state;
    ATF_REQUIRE(luaL_dostring(raw(state),
                              "the_table = { foo = 1, bar = 2 }") == 0);
    lua_getglobal(raw(state), "the_table");
    lua_pushstring(raw(state), "baz");
    state.get_table();
    ATF_REQUIRE(lua_isnil(raw(state), -1));
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__get_top);
ATF_TEST_CASE_BODY(state__get_top)
{
    lua::state state;
    ATF_REQUIRE_EQ(0, state.get_top());
    lua_pushinteger(raw(state), 3);
    ATF_REQUIRE_EQ(1, state.get_top());
    lua_pushinteger(raw(state), 3);
    ATF_REQUIRE_EQ(2, state.get_top());
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__is_boolean__empty);
ATF_TEST_CASE_BODY(state__is_boolean__empty)
{
    lua::state state;
    ATF_REQUIRE(!state.is_boolean());
}


ATF_TEST_CASE_WITHOUT_HEAD(state__is_boolean__top);
ATF_TEST_CASE_BODY(state__is_boolean__top)
{
    lua::state state;
    lua_pushnil(raw(state));
    ATF_REQUIRE(!state.is_boolean());
    lua_pushboolean(raw(state), 1);
    ATF_REQUIRE(state.is_boolean());
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__is_boolean__explicit);
ATF_TEST_CASE_BODY(state__is_boolean__explicit)
{
    lua::state state;
    lua_pushboolean(raw(state), 1);
    ATF_REQUIRE(state.is_boolean(-1));
    lua_pushinteger(raw(state), 5);
    ATF_REQUIRE(!state.is_boolean(-1));
    ATF_REQUIRE(state.is_boolean(-2));
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__is_function__empty);
ATF_TEST_CASE_BODY(state__is_function__empty)
{
    lua::state state;
    ATF_REQUIRE(!state.is_function());
}


ATF_TEST_CASE_WITHOUT_HEAD(state__is_function__top);
ATF_TEST_CASE_BODY(state__is_function__top)
{
    lua::state state;
    luaL_dostring(raw(state), "function my_func(a, b) return a + b; end");

    lua_pushnil(raw(state));
    ATF_REQUIRE(!state.is_function());
    lua_getglobal(raw(state), "my_func");
    ATF_REQUIRE(state.is_function());
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__is_function__explicit);
ATF_TEST_CASE_BODY(state__is_function__explicit)
{
    lua::state state;
    luaL_dostring(raw(state), "function my_func(a, b) return a + b; end");

    lua_getglobal(raw(state), "my_func");
    ATF_REQUIRE(state.is_function(-1));
    lua_pushinteger(raw(state), 5);
    ATF_REQUIRE(!state.is_function(-1));
    ATF_REQUIRE(state.is_function(-2));
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__is_nil__empty);
ATF_TEST_CASE_BODY(state__is_nil__empty)
{
    lua::state state;
    ATF_REQUIRE(state.is_nil());
}


ATF_TEST_CASE_WITHOUT_HEAD(state__is_nil__top);
ATF_TEST_CASE_BODY(state__is_nil__top)
{
    lua::state state;
    lua_pushnil(raw(state));
    ATF_REQUIRE(state.is_nil());
    lua_pushinteger(raw(state), 5);
    ATF_REQUIRE(!state.is_nil());
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__is_nil__explicit);
ATF_TEST_CASE_BODY(state__is_nil__explicit)
{
    lua::state state;
    lua_pushnil(raw(state));
    ATF_REQUIRE(state.is_nil(-1));
    lua_pushinteger(raw(state), 5);
    ATF_REQUIRE(!state.is_nil(-1));
    ATF_REQUIRE(state.is_nil(-2));
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__is_number__empty);
ATF_TEST_CASE_BODY(state__is_number__empty)
{
    lua::state state;
    ATF_REQUIRE(!state.is_number());
}


ATF_TEST_CASE_WITHOUT_HEAD(state__is_number__top);
ATF_TEST_CASE_BODY(state__is_number__top)
{
    lua::state state;
    lua_pushnil(raw(state));
    ATF_REQUIRE(!state.is_number());
    lua_pushinteger(raw(state), 5);
    ATF_REQUIRE(state.is_number());
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__is_number__explicit);
ATF_TEST_CASE_BODY(state__is_number__explicit)
{
    lua::state state;
    lua_pushnil(raw(state));
    ATF_REQUIRE(!state.is_number(-1));
    lua_pushinteger(raw(state), 5);
    ATF_REQUIRE(state.is_number(-1));
    ATF_REQUIRE(!state.is_number(-2));
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__is_string__empty);
ATF_TEST_CASE_BODY(state__is_string__empty)
{
    lua::state state;
    ATF_REQUIRE(!state.is_string());
}


ATF_TEST_CASE_WITHOUT_HEAD(state__is_string__top);
ATF_TEST_CASE_BODY(state__is_string__top)
{
    lua::state state;
    lua_pushnil(raw(state));
    ATF_REQUIRE(!state.is_string());
    lua_pushinteger(raw(state), 3);
    ATF_REQUIRE(state.is_string());
    lua_pushstring(raw(state), "foo");
    ATF_REQUIRE(state.is_string());
    lua_pop(raw(state), 3);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__is_string__explicit);
ATF_TEST_CASE_BODY(state__is_string__explicit)
{
    lua::state state;
    lua_pushinteger(raw(state), 3);
    ATF_REQUIRE(state.is_string(-1));
    lua_pushnil(raw(state));
    ATF_REQUIRE(!state.is_string(-1));
    ATF_REQUIRE(state.is_string(-2));
    lua_pushstring(raw(state), "foo");
    ATF_REQUIRE(state.is_string(-1));
    ATF_REQUIRE(!state.is_string(-2));
    ATF_REQUIRE(state.is_string(-3));
    lua_pop(raw(state), 3);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__is_table__empty);
ATF_TEST_CASE_BODY(state__is_table__empty)
{
    lua::state state;
    ATF_REQUIRE(!state.is_table());
}


ATF_TEST_CASE_WITHOUT_HEAD(state__is_table__top);
ATF_TEST_CASE_BODY(state__is_table__top)
{
    lua::state state;
    luaL_dostring(raw(state), "t = {3, 4, 5}");

    lua_pushstring(raw(state), "foo");
    ATF_REQUIRE(!state.is_table());
    lua_getglobal(raw(state), "t");
    ATF_REQUIRE(state.is_table());
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__is_table__explicit);
ATF_TEST_CASE_BODY(state__is_table__explicit)
{
    lua::state state;
    luaL_dostring(raw(state), "t = {3, 4, 5}");

    lua_pushstring(raw(state), "foo");
    ATF_REQUIRE(!state.is_table(-1));
    lua_getglobal(raw(state), "t");
    ATF_REQUIRE(state.is_table(-1));
    ATF_REQUIRE(!state.is_table(-2));
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__load_file__ok);
ATF_TEST_CASE_BODY(state__load_file__ok)
{
    std::ofstream output("test.lua");
    output << "in_the_file = \"oh yes\"\n";
    output.close();

    lua::state state;
    state.load_file(fs::path("test.lua"));
    ATF_REQUIRE(lua_pcall(raw(state), 0, 0, 0) == 0);
    lua_getglobal(raw(state), "in_the_file");
    ATF_REQUIRE(std::strcmp("oh yes", lua_tostring(raw(state), -1)) == 0);
    lua_pop(raw(state), 1);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__load_file__api_error);
ATF_TEST_CASE_BODY(state__load_file__api_error)
{
    std::ofstream output("test.lua");
    output << "I have a bad syntax!  Wohoo!\n";
    output.close();

    lua::state state;
    REQUIRE_API_ERROR("luaL_loadfile",
                      state.load_file(fs::path("test.lua")));
}


ATF_TEST_CASE_WITHOUT_HEAD(state__load_file__file_not_found_error);
ATF_TEST_CASE_BODY(state__load_file__file_not_found_error)
{
    lua::state state;
    ATF_REQUIRE_THROW_RE(lua::file_not_found_error, "missing.lua",
                         state.load_file(fs::path("missing.lua")));
}


ATF_TEST_CASE_WITHOUT_HEAD(state__load_string__ok);
ATF_TEST_CASE_BODY(state__load_string__ok)
{
    lua::state state;
    state.load_string("return 2 + 3");
    ATF_REQUIRE(lua_pcall(raw(state), 0, 1, 0) == 0);
    ATF_REQUIRE_EQ(5, lua_tointeger(raw(state), -1));
    lua_pop(raw(state), 1);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__load_string__fail);
ATF_TEST_CASE_BODY(state__load_string__fail)
{
    lua::state state;
    REQUIRE_API_ERROR("luaL_loadstring", state.load_string("-"));
}


ATF_TEST_CASE_WITHOUT_HEAD(state__new_table);
ATF_TEST_CASE_BODY(state__new_table)
{
    lua::state state;
    state.new_table();
    ATF_REQUIRE_EQ(1, lua_gettop(raw(state)));
    ATF_REQUIRE(lua_istable(raw(state), -1));
    lua_pop(raw(state), 1);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__open_base);
ATF_TEST_CASE_BODY(state__open_base)
{
    lua::state state;
    check_modules(state, "");
    state.open_base();
    check_modules(state, "base");
}


ATF_TEST_CASE_WITHOUT_HEAD(state__open_string);
ATF_TEST_CASE_BODY(state__open_string)
{
    lua::state state;
    check_modules(state, "");
    state.open_string();
    check_modules(state, "string");
}


ATF_TEST_CASE_WITHOUT_HEAD(state__open_table);
ATF_TEST_CASE_BODY(state__open_table)
{
    lua::state state;
    check_modules(state, "");
    state.open_table();
    check_modules(state, "table");
}


ATF_TEST_CASE_WITHOUT_HEAD(state__pcall__ok);
ATF_TEST_CASE_BODY(state__pcall__ok)
{
    lua::state state;
    luaL_loadstring(raw(state), "function mul(a, b) return a * b; end");
    state.pcall(0, 0, 0);
    lua_getfield(raw(state), LUA_GLOBALSINDEX, "mul");
    lua_pushinteger(raw(state), 3);
    lua_pushinteger(raw(state), 5);
    state.pcall(2, 1, 0);
    ATF_REQUIRE_EQ(15, lua_tointeger(raw(state), -1));
    lua_pop(raw(state), 1);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__pcall__fail);
ATF_TEST_CASE_BODY(state__pcall__fail)
{
    lua::state state;
    lua_pushnil(raw(state));
    REQUIRE_API_ERROR("lua_pcall", state.pcall(0, 0, 0));
}


ATF_TEST_CASE_WITHOUT_HEAD(state__pop__one);
ATF_TEST_CASE_BODY(state__pop__one)
{
    lua::state state;
    lua_pushinteger(raw(state), 10);
    lua_pushinteger(raw(state), 20);
    lua_pushinteger(raw(state), 30);
    state.pop(1);
    ATF_REQUIRE_EQ(2, lua_gettop(raw(state)));
    ATF_REQUIRE_EQ(20, lua_tointeger(raw(state), -1));
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__pop__many);
ATF_TEST_CASE_BODY(state__pop__many)
{
    lua::state state;
    lua_pushinteger(raw(state), 10);
    lua_pushinteger(raw(state), 20);
    lua_pushinteger(raw(state), 30);
    state.pop(2);
    ATF_REQUIRE_EQ(1, lua_gettop(raw(state)));
    ATF_REQUIRE_EQ(10, lua_tointeger(raw(state), -1));
    lua_pop(raw(state), 1);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__push_boolean);
ATF_TEST_CASE_BODY(state__push_boolean)
{
    lua::state state;
    state.push_boolean(true);
    ATF_REQUIRE_EQ(1, lua_gettop(raw(state)));
    ATF_REQUIRE(lua_toboolean(raw(state), -1));
    state.push_boolean(false);
    ATF_REQUIRE_EQ(2, lua_gettop(raw(state)));
    ATF_REQUIRE(!lua_toboolean(raw(state), -1));
    ATF_REQUIRE(lua_toboolean(raw(state), -2));
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__push_c_function__c_ok);
ATF_TEST_CASE_BODY(state__push_c_function__c_ok)
{
    lua::state state;
    state.push_c_function(c_multiply);
    lua_setglobal(raw(state), "c_multiply");

    ATF_REQUIRE(luaL_dostring(raw(state), "return c_multiply(3, 4)") == 0);
    ATF_REQUIRE_EQ(12, lua_tointeger(raw(state), -1));
    lua_pop(raw(state), 1);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__push_c_function__cxx_ok);
ATF_TEST_CASE_BODY(state__push_c_function__cxx_ok)
{
    lua::state state;
    state.push_c_function(lua::wrap_cxx_function< cxx_divide >);
    lua_setglobal(raw(state), "cxx_divide");

    ATF_REQUIRE(luaL_dostring(raw(state), "return cxx_divide(17, 3)") == 0);
    ATF_REQUIRE_EQ(5, lua_tointeger(raw(state), -2));
    ATF_REQUIRE_EQ(2, lua_tointeger(raw(state), -1));
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__push_c_function__cxx_fail_exception);
ATF_TEST_CASE_BODY(state__push_c_function__cxx_fail_exception)
{
    lua::state state;
    state.push_c_function(lua::wrap_cxx_function< cxx_divide >);
    lua_setglobal(raw(state), "cxx_divide");

    ATF_REQUIRE(luaL_dostring(raw(state), "return cxx_divide(15, 0)") != 0);
    ATF_REQUIRE_MATCH("Divisor is 0", lua_tostring(raw(state), -1));
    lua_pop(raw(state), 1);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__push_c_function__cxx_fail_anything);
ATF_TEST_CASE_BODY(state__push_c_function__cxx_fail_anything)
{
    lua::state state;
    state.push_c_function(lua::wrap_cxx_function< cxx_divide >);
    lua_setglobal(raw(state), "cxx_divide");

    ATF_REQUIRE(luaL_dostring(raw(state), "return cxx_divide(-3, -1)") != 0);
    ATF_REQUIRE_MATCH("Unhandled exception", lua_tostring(raw(state), -1));
    lua_pop(raw(state), 1);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__push_c_function__cxx_fail_overflow);
ATF_TEST_CASE_BODY(state__push_c_function__cxx_fail_overflow)
{
    lua::state state;
    state.push_c_function(lua::wrap_cxx_function< raise_long_error >);
    lua_setglobal(raw(state), "fail");

    ATF_REQUIRE(luaL_dostring(raw(state), "return fail(900)") != 0);
    ATF_REQUIRE_MATCH(std::string(900, 'A'), lua_tostring(raw(state), -1));
    lua_pop(raw(state), 1);

    ATF_REQUIRE(luaL_dostring(raw(state), "return fail(8192)") != 0);
    ATF_REQUIRE_MATCH(std::string(900, 'A'), lua_tostring(raw(state), -1));
    lua_pop(raw(state), 1);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__push_integer);
ATF_TEST_CASE_BODY(state__push_integer)
{
    lua::state state;
    state.push_integer(12);
    ATF_REQUIRE_EQ(1, lua_gettop(raw(state)));
    ATF_REQUIRE_EQ(12, lua_tointeger(raw(state), -1));
    state.push_integer(34);
    ATF_REQUIRE_EQ(2, lua_gettop(raw(state)));
    ATF_REQUIRE_EQ(34, lua_tointeger(raw(state), -1));
    ATF_REQUIRE_EQ(12, lua_tointeger(raw(state), -2));
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__push_string);
ATF_TEST_CASE_BODY(state__push_string)
{
    lua::state state;

    {
        std::string str = "first";
        state.push_string(str);
        ATF_REQUIRE_EQ(1, lua_gettop(raw(state)));
        ATF_REQUIRE_EQ(std::string("first"), lua_tostring(raw(state), -1));
        str = "second";
        state.push_string(str);
    }
    ATF_REQUIRE_EQ(2, lua_gettop(raw(state)));
    ATF_REQUIRE_EQ(std::string("second"), lua_tostring(raw(state), -1));
    ATF_REQUIRE_EQ(std::string("first"), lua_tostring(raw(state), -2));
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__set_global__ok);
ATF_TEST_CASE_BODY(state__set_global__ok)
{
    lua::state state;
    lua_pushinteger(raw(state), 3);
    state.set_global("test_variable");
    ATF_REQUIRE(luaL_dostring(raw(state), "return test_variable + 1") == 0);
    ATF_REQUIRE(lua_isnumber(raw(state), -1));
    ATF_REQUIRE_EQ(4, lua_tointeger(raw(state), -1));
    lua_pop(raw(state), 1);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__set_global__fail);
ATF_TEST_CASE_BODY(state__set_global__fail)
{
    lua::state state;
    lua_pushinteger(raw(state), 3);
    lua_replace(raw(state), LUA_GLOBALSINDEX);
    lua_pushinteger(raw(state), 4);
    REQUIRE_API_ERROR("lua_setglobal", state.set_global("test_variable"));
    lua_pop(raw(state), 1);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__set_table__ok);
ATF_TEST_CASE_BODY(state__set_table__ok)
{
    lua::state state;
    ATF_REQUIRE(luaL_dostring(raw(state), "t = { a = 1, bar = 234 }") == 0);
    lua_getglobal(raw(state), "t");

    lua_pushstring(raw(state), "bar");
    lua_pushstring(raw(state), "baz");
    state.set_table();
    ATF_REQUIRE_EQ(1, lua_gettop(raw(state)));

    lua_pushstring(raw(state), "a");
    lua_gettable(raw(state), -2);
    ATF_REQUIRE(lua_isnumber(raw(state), -1));
    ATF_REQUIRE_EQ(1, lua_tointeger(raw(state), -1));
    lua_pop(raw(state), 1);

    lua_pushstring(raw(state), "bar");
    lua_gettable(raw(state), -2);
    ATF_REQUIRE(lua_isstring(raw(state), -1));
    ATF_REQUIRE_EQ(std::string("baz"), lua_tostring(raw(state), -1));
    lua_pop(raw(state), 1);

    lua_pop(raw(state), 1);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__set_table__nil);
ATF_TEST_CASE_BODY(state__set_table__nil)
{
    lua::state state;
    lua_pushnil(raw(state));
    lua_pushinteger(raw(state), 1);
    lua_pushinteger(raw(state), 2);
    REQUIRE_API_ERROR("lua_settable", state.set_table(-3));
    lua_pop(raw(state), 3);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__to_boolean__top);
ATF_TEST_CASE_BODY(state__to_boolean__top)
{
    lua::state state;
    lua_pushboolean(raw(state), 1);
    ATF_REQUIRE(state.to_boolean());
    lua_pushboolean(raw(state), 0);
    ATF_REQUIRE(!state.to_boolean());
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__to_boolean__explicit);
ATF_TEST_CASE_BODY(state__to_boolean__explicit)
{
    lua::state state;
    lua_pushboolean(raw(state), 0);
    lua_pushboolean(raw(state), 1);
    ATF_REQUIRE(!state.to_boolean(-2));
    ATF_REQUIRE(state.to_boolean(-1));
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__to_integer__top);
ATF_TEST_CASE_BODY(state__to_integer__top)
{
    lua::state state;
    lua_pushstring(raw(state), "34");
    ATF_REQUIRE_EQ(34, state.to_integer());
    lua_pushinteger(raw(state), 12);
    ATF_REQUIRE_EQ(12, state.to_integer());
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__to_integer__explicit);
ATF_TEST_CASE_BODY(state__to_integer__explicit)
{
    lua::state state;
    lua_pushinteger(raw(state), 12);
    lua_pushstring(raw(state), "foobar");
    ATF_REQUIRE_EQ(12, state.to_integer(-2));
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__to_string__top);
ATF_TEST_CASE_BODY(state__to_string__top)
{
    lua::state state;
    lua_pushstring(raw(state), "foobar");
    ATF_REQUIRE_EQ("foobar", state.to_string());
    lua_pushinteger(raw(state), 12);
    ATF_REQUIRE_EQ("12", state.to_string());
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(state__to_string__explicit);
ATF_TEST_CASE_BODY(state__to_string__explicit)
{
    lua::state state;
    lua_pushstring(raw(state), "foobar");
    lua_pushinteger(raw(state), 12);
    ATF_REQUIRE_EQ("foobar", state.to_string(-2));
    ATF_REQUIRE_EQ("12", state.to_string(-1));
    lua_pop(raw(state), 2);
}


ATF_TEST_CASE_WITHOUT_HEAD(stack_cleaner__empty);
ATF_TEST_CASE_BODY(stack_cleaner__empty)
{
    lua::state state;
    {
        lua::stack_cleaner cleaner(state);
        ATF_REQUIRE_EQ(0, state.get_top());
    }
    ATF_REQUIRE_EQ(0, state.get_top());
}


ATF_TEST_CASE_WITHOUT_HEAD(stack_cleaner__some);
ATF_TEST_CASE_BODY(stack_cleaner__some)
{
    lua::state state;
    {
        lua::stack_cleaner cleaner(state);
        state.push_integer(15);
        ATF_REQUIRE_EQ(1, state.get_top());
        state.push_integer(30);
        ATF_REQUIRE_EQ(2, state.get_top());
    }
    ATF_REQUIRE_EQ(0, state.get_top());
}


ATF_TEST_CASE_WITHOUT_HEAD(stack_cleaner__nested);
ATF_TEST_CASE_BODY(stack_cleaner__nested)
{
    lua::state state;
    {
        lua::stack_cleaner cleaner1(state);
        state.push_integer(10);
        ATF_REQUIRE_EQ(1, state.get_top());
        ATF_REQUIRE_EQ(10, state.to_integer());
        {
            lua::stack_cleaner cleaner2(state);
            state.push_integer(20);
            ATF_REQUIRE_EQ(2, state.get_top());
            ATF_REQUIRE_EQ(20, state.to_integer(-1));
            ATF_REQUIRE_EQ(10, state.to_integer(-2));
        }
        ATF_REQUIRE_EQ(1, state.get_top());
        ATF_REQUIRE_EQ(10, state.to_integer());
    }
    ATF_REQUIRE_EQ(0, state.get_top());
}


ATF_TEST_CASE_WITHOUT_HEAD(stack_cleaner__forget);
ATF_TEST_CASE_BODY(stack_cleaner__forget)
{
    lua::state state;
    {
        lua::stack_cleaner cleaner(state);
        state.push_integer(15);
        state.push_integer(30);
        cleaner.forget();
        state.push_integer(60);
        ATF_REQUIRE_EQ(3, state.get_top());
    }
    ATF_REQUIRE_EQ(2, state.get_top());
    ATF_REQUIRE_EQ(30, state.to_integer());
    state.pop(2);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, state__ctor_only_wrap);
    ATF_ADD_TEST_CASE(tcs, state__close);
    ATF_ADD_TEST_CASE(tcs, state__get_global__ok);
    ATF_ADD_TEST_CASE(tcs, state__get_global__fail);
    ATF_ADD_TEST_CASE(tcs, state__get_global__undefined);
    ATF_ADD_TEST_CASE(tcs, state__get_table__ok);
    ATF_ADD_TEST_CASE(tcs, state__get_table__nil);
    ATF_ADD_TEST_CASE(tcs, state__get_table__unknown_index);
    ATF_ADD_TEST_CASE(tcs, state__get_top);
    ATF_ADD_TEST_CASE(tcs, state__is_boolean__empty);
    ATF_ADD_TEST_CASE(tcs, state__is_boolean__top);
    ATF_ADD_TEST_CASE(tcs, state__is_boolean__explicit);
    ATF_ADD_TEST_CASE(tcs, state__is_function__empty);
    ATF_ADD_TEST_CASE(tcs, state__is_function__top);
    ATF_ADD_TEST_CASE(tcs, state__is_function__explicit);
    ATF_ADD_TEST_CASE(tcs, state__is_nil__empty);
    ATF_ADD_TEST_CASE(tcs, state__is_nil__top);
    ATF_ADD_TEST_CASE(tcs, state__is_nil__explicit);
    ATF_ADD_TEST_CASE(tcs, state__is_number__empty);
    ATF_ADD_TEST_CASE(tcs, state__is_number__top);
    ATF_ADD_TEST_CASE(tcs, state__is_number__explicit);
    ATF_ADD_TEST_CASE(tcs, state__is_string__empty);
    ATF_ADD_TEST_CASE(tcs, state__is_string__top);
    ATF_ADD_TEST_CASE(tcs, state__is_string__explicit);
    ATF_ADD_TEST_CASE(tcs, state__is_table__empty);
    ATF_ADD_TEST_CASE(tcs, state__is_table__top);
    ATF_ADD_TEST_CASE(tcs, state__is_table__explicit);
    ATF_ADD_TEST_CASE(tcs, state__load_file__ok);
    ATF_ADD_TEST_CASE(tcs, state__load_file__api_error);
    ATF_ADD_TEST_CASE(tcs, state__load_file__file_not_found_error);
    ATF_ADD_TEST_CASE(tcs, state__load_string__ok);
    ATF_ADD_TEST_CASE(tcs, state__load_string__fail);
    ATF_ADD_TEST_CASE(tcs, state__new_table);
    ATF_ADD_TEST_CASE(tcs, state__open_base);
    ATF_ADD_TEST_CASE(tcs, state__open_string);
    ATF_ADD_TEST_CASE(tcs, state__open_table);
    ATF_ADD_TEST_CASE(tcs, state__pcall__ok);
    ATF_ADD_TEST_CASE(tcs, state__pcall__fail);
    ATF_ADD_TEST_CASE(tcs, state__pop__one);
    ATF_ADD_TEST_CASE(tcs, state__pop__many);
    ATF_ADD_TEST_CASE(tcs, state__push_boolean);
    ATF_ADD_TEST_CASE(tcs, state__push_c_function__c_ok);
    ATF_ADD_TEST_CASE(tcs, state__push_c_function__cxx_ok);
    ATF_ADD_TEST_CASE(tcs, state__push_c_function__cxx_fail_exception);
    ATF_ADD_TEST_CASE(tcs, state__push_c_function__cxx_fail_anything);
    ATF_ADD_TEST_CASE(tcs, state__push_c_function__cxx_fail_overflow);
    ATF_ADD_TEST_CASE(tcs, state__push_integer);
    ATF_ADD_TEST_CASE(tcs, state__push_string);
    ATF_ADD_TEST_CASE(tcs, state__set_global__ok);
    ATF_ADD_TEST_CASE(tcs, state__set_global__fail);
    ATF_ADD_TEST_CASE(tcs, state__set_table__ok);
    ATF_ADD_TEST_CASE(tcs, state__set_table__nil);
    ATF_ADD_TEST_CASE(tcs, state__to_boolean__top);
    ATF_ADD_TEST_CASE(tcs, state__to_boolean__explicit);
    ATF_ADD_TEST_CASE(tcs, state__to_integer__top);
    ATF_ADD_TEST_CASE(tcs, state__to_integer__explicit);
    ATF_ADD_TEST_CASE(tcs, state__to_string__top);
    ATF_ADD_TEST_CASE(tcs, state__to_string__explicit);

    ATF_ADD_TEST_CASE(tcs, stack_cleaner__empty);
    ATF_ADD_TEST_CASE(tcs, stack_cleaner__some);
    ATF_ADD_TEST_CASE(tcs, stack_cleaner__nested);
    ATF_ADD_TEST_CASE(tcs, stack_cleaner__forget);
}
