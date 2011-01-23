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

// TODO(jmmv): These tests ought to be written in Lua.  Rewrite when we have a
// Lua binding.

#include <fstream>

#include <atf-c++.hpp>

#include "engine/user_files/common.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/lua/exceptions.hpp"
#include "utils/lua/operations.hpp"
#include "utils/lua/wrap.hpp"

namespace fs = utils::fs;
namespace lua = utils::lua;
namespace user_files = engine::user_files;


namespace {


/// Creates a mock module that can be called from syntax().
///
/// \param file The name of the file to create.
/// \param loaded_cookie A value that will be set in the global 'loaded_cookie'
///     variable within Lua to validate that nesting of module loading works
///     properly.
static void
create_mock_module(const char* file, const char* loaded_cookie)
{
    std::ofstream output(file);
    ATF_REQUIRE(output);
    output << F("return {export=function() _G.loaded_cookie = '%s' end}\n") %
        loaded_cookie;
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(get_filename);
ATF_TEST_CASE_BODY(get_filename)
{
    lua::state state;
    user_files::init(state, fs::path("this/is/my-name"), "/non-existent");

    lua::eval(state, "init.get_filename()");
    ATF_REQUIRE_EQ("this/is/my-name", state.to_string());
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(get_syntax__ok);
ATF_TEST_CASE_BODY(get_syntax__ok)
{
    lua::state state;
    user_files::init(state, fs::path("this/is/my-name"), ".");

    create_mock_module("kyuafile_1.lua", "unused");
    lua::do_string(state, "syntax('kyuafile', 1)");

    lua::eval(state, "init.get_syntax().format");
    ATF_REQUIRE_EQ("kyuafile", state.to_string());
    lua::eval(state, "init.get_syntax().version");
    ATF_REQUIRE_EQ(1, state.to_integer());
    state.pop(2);
}


ATF_TEST_CASE_WITHOUT_HEAD(get_syntax__fail);
ATF_TEST_CASE_BODY(get_syntax__fail)
{
    lua::state state;
    user_files::init(state, fs::path("the-name"), "/non-existent");

    ATF_REQUIRE_THROW_RE(lua::error, "Syntax not defined in file 'the-name'",
                         lua::eval(state, "init.get_syntax()"));
}


ATF_TEST_CASE_WITHOUT_HEAD(run__simple);
ATF_TEST_CASE_BODY(run__simple)
{
    lua::state state;
    user_files::init(state, fs::path("root.lua"));

    std::ofstream output("simple.lua");
    ATF_REQUIRE(output);
    output << "global_variable = 54321\n";
    output.close();

    lua::do_string(state, "simple_env = init.run('simple.lua')");

    state.get_global("global_variable");
    ATF_REQUIRE(state.is_nil());
    state.pop(1);

    lua::eval(state, "simple_env.global_variable");
    ATF_REQUIRE_EQ(54321, state.to_integer());
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(run__chain);
ATF_TEST_CASE_BODY(run__chain)
{
    lua::state state;
    user_files::init(state, fs::path("root.lua"));

    {
        std::ofstream output("simple1.lua");
        ATF_REQUIRE(output);
        output << "global_variable = 1\n";
        output << "env2 = init.run('simple2.lua')\n";
    }

    {
        std::ofstream output("simple2.lua");
        ATF_REQUIRE(output);
        output << "syntax('kyuafile', 1)\n";
        output << "global_variable = 2\n";
    }

    lua::do_string(state, "env1 = init.run('simple1.lua')");

    lua::do_string(state, "assert(global_variable == nil)");
    lua::do_string(state, "assert(env1.global_variable == 1)");
    lua::do_string(state, "assert(env1.env2.global_variable == 2)");

    ATF_REQUIRE_THROW(lua::error, lua::do_string(state, "init.get_syntax()"));
    ATF_REQUIRE_THROW(lua::error,
                      lua::do_string(state, "init.env1.get_syntax()"));
    lua::do_string(state,
                   "assert(env1.env2.init.get_syntax().format == 'kyuafile')");
    lua::do_string(state,
                   "assert(env1.env2.init.get_syntax().version == 1)");
}


ATF_TEST_CASE_WITHOUT_HEAD(syntax__kyuafile_1__ok);
ATF_TEST_CASE_BODY(syntax__kyuafile_1__ok)
{
    lua::state state;
    user_files::init(state, fs::path("the-file"), ".");

    create_mock_module("kyuafile_1.lua", "i-am-the-kyuafile");
    lua::do_string(state, "syntax('kyuafile', 1)");

    lua::eval(state, "init.get_syntax().format");
    ATF_REQUIRE_EQ("kyuafile", state.to_string());
    lua::eval(state, "init.get_syntax().version");
    ATF_REQUIRE_EQ(1, state.to_integer());
    lua::eval(state, "loaded_cookie");
    ATF_REQUIRE_EQ("i-am-the-kyuafile", state.to_string());
    state.pop(3);
}


ATF_TEST_CASE_WITHOUT_HEAD(syntax__kyuafile_1__version_error);
ATF_TEST_CASE_BODY(syntax__kyuafile_1__version_error)
{
    lua::state state;
    user_files::init(state, fs::path("the-file"), ".");

    create_mock_module("kyuafile_1.lua", "unused");
    ATF_REQUIRE_THROW_RE(lua::error, "Syntax request error: unknown version 2 "
                         "for format 'kyuafile'",
                         lua::do_string(state, "syntax('kyuafile', 2)"));

    ATF_REQUIRE_THROW_RE(lua::error, "not defined",
                         lua::eval(state, "init.get_syntax()"));

    lua::eval(state, "loaded_cookie");
    ATF_REQUIRE(state.is_nil());
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(syntax__kyuafile_1__missing_file);
ATF_TEST_CASE_BODY(syntax__kyuafile_1__missing_file)
{
    lua::state state;
    user_files::init(state, fs::path("the-file"), ".");

    ATF_REQUIRE_THROW_RE(lua::error, "kyuafile_1.lua",
                         lua::do_string(state, "syntax('kyuafile', 1)"));

    ATF_REQUIRE_THROW_RE(lua::error, "not defined",
                         lua::eval(state, "init.get_syntax()"));

    lua::eval(state, "loaded_cookie");
    ATF_REQUIRE(state.is_nil());
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(syntax__format_error);
ATF_TEST_CASE_BODY(syntax__format_error)
{
    lua::state state;
    user_files::init(state, fs::path("the-file"), ".");

    create_mock_module("kyuafile_1.lua", "unused");
    ATF_REQUIRE_THROW_RE(lua::error, "Syntax request error: unknown format "
                         "'foo'", lua::do_string(state, "syntax('foo', 123)"));

    ATF_REQUIRE_THROW_RE(lua::error, "not defined",
                         lua::eval(state, "init.get_syntax()"));

    lua::eval(state, "loaded_cookie");
    ATF_REQUIRE(state.is_nil());
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(syntax__twice);
ATF_TEST_CASE_BODY(syntax__twice)
{
    lua::state state;
    user_files::init(state, fs::path("the-file"), ".");

    create_mock_module("kyuafile_1.lua", "unused");
    ATF_REQUIRE_THROW_RE(lua::error, "syntax.*more than once",
                         lua::do_string(state, "syntax('kyuafile', 1); "
                                        "syntax('a', 3)"));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, get_filename);

    ATF_ADD_TEST_CASE(tcs, get_syntax__ok);
    ATF_ADD_TEST_CASE(tcs, get_syntax__fail);

    ATF_ADD_TEST_CASE(tcs, run__simple);
    ATF_ADD_TEST_CASE(tcs, run__chain);

    ATF_ADD_TEST_CASE(tcs, syntax__kyuafile_1__ok);
    ATF_ADD_TEST_CASE(tcs, syntax__kyuafile_1__version_error);
    ATF_ADD_TEST_CASE(tcs, syntax__kyuafile_1__missing_file);
    ATF_ADD_TEST_CASE(tcs, syntax__format_error);
    ATF_ADD_TEST_CASE(tcs, syntax__twice);
}
