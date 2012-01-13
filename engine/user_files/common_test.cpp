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

#include "engine/user_files/common.hpp"

#include <fstream>

#include <atf-c++.hpp>
#include <lutok/exceptions.hpp>
#include <lutok/operations.hpp>
#include <lutok/state.hpp>

#include "utils/fs/path.hpp"

namespace fs = utils::fs;
namespace user_files = engine::user_files;


namespace {


/// Creates a mock module that can be called from syntax().
///
/// \param file The name of the file to create.
static void
create_mock_module(const char* file)
{
    std::ofstream output(file);
    ATF_REQUIRE(output);
    output << "return {export=function() end}\n";
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(do_user_file__simple);
ATF_TEST_CASE_BODY(do_user_file__simple)
{
    std::ofstream output("simple.lua");
    ATF_REQUIRE(output);
    output << "syntax('kyuafile', 1)\n";
    output << "my_global = 'good-to-go!'\n";
    output.close();

    lutok::state state;
    create_mock_module("kyuafile_1.lua");
    const user_files::syntax_def syntax = user_files::do_user_file(
        state, fs::path("simple.lua"));
    ATF_REQUIRE_EQ("kyuafile", syntax.first);
    ATF_REQUIRE_EQ(1, syntax.second);
    lutok::do_string(state, "assert(my_global == 'good-to-go!')");
    lutok::do_string(state, "assert(init.get_filename() == 'simple.lua')");
}


ATF_TEST_CASE_WITHOUT_HEAD(do_user_file__missing_syntax);
ATF_TEST_CASE_BODY(do_user_file__missing_syntax)
{
    std::ofstream output("simple.lua");
    ATF_REQUIRE(output);
    output << "my_global = 'oh, no'\n";
    output.close();

    lutok::state state;
    ATF_REQUIRE_THROW_RE(lutok::error, "Syntax not defined",
                         user_files::do_user_file(state,
                                                  fs::path("simple.lua")));
    lutok::do_string(state, "assert(my_global == 'oh, no')");
}


ATF_TEST_CASE_WITHOUT_HEAD(do_user_file__missing_file);
ATF_TEST_CASE_BODY(do_user_file__missing_file)
{
    lutok::state state;
    ATF_REQUIRE_THROW(lutok::error, user_files::do_user_file(
        state, fs::path("non-existent.lua")));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_syntax__ok);
ATF_TEST_CASE_BODY(get_syntax__ok)
{
    std::ofstream output("simple.lua");
    ATF_REQUIRE(output);
    output << "syntax('kyuafile', 1)\n";
    output.close();

    lutok::state state;
    create_mock_module("kyuafile_1.lua");
    const std::pair< std::string, int > returned_syntax =
        user_files::do_user_file(state, fs::path("simple.lua"));
    const std::pair< std::string, int > syntax = user_files::get_syntax(state);
    ATF_REQUIRE(returned_syntax == syntax);
    ATF_REQUIRE_EQ("kyuafile", syntax.first);
    ATF_REQUIRE_EQ(1, syntax.second);
}


ATF_TEST_CASE_WITHOUT_HEAD(get_syntax__no_table);
ATF_TEST_CASE_BODY(get_syntax__no_table)
{
    lutok::state state;
    lutok::do_string(state, "init = {}; "
                     "function init.get_syntax() return nil; end");
    ATF_REQUIRE_THROW_RE(lutok::error, "not a table",
                         user_files::get_syntax(state));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_syntax__not_defined);
ATF_TEST_CASE_BODY(get_syntax__not_defined)
{
    lutok::state state;
    lutok::do_string(state, "init = {}; "
                     "syntax = {format=nil, version=nil}; "
                     "function init.get_syntax() return syntax; end");
    ATF_REQUIRE_THROW_RE(lutok::error, "not defined",
                         user_files::get_syntax(state));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_syntax__bad_format);
ATF_TEST_CASE_BODY(get_syntax__bad_format)
{
    lutok::state state;
    lutok::do_string(state, "init = {}; "
                     "syntax = {format={}, version=1}; "
                     "function init.get_syntax() return syntax; end");
    ATF_REQUIRE_THROW_RE(lutok::error, "format.*not a string",
                         user_files::get_syntax(state));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_syntax__bad_version);
ATF_TEST_CASE_BODY(get_syntax__bad_version)
{
    lutok::state state;
    lutok::do_string(state, "init = {}; "
                     "syntax = {format='foo', version={}}; "
                     "function init.get_syntax() return syntax; end");
    ATF_REQUIRE_THROW_RE(lutok::error, "version.*not an integer",
                         user_files::get_syntax(state));
}


ATF_TEST_CASE_WITHOUT_HEAD(init__ok);
ATF_TEST_CASE_BODY(init__ok)
{
    lutok::state state;
    user_files::init(state, fs::path("non-existent.lua"));
    lutok::do_string(state,
                     "assert(init.get_filename() == 'non-existent.lua')");
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, do_user_file__simple);
    ATF_ADD_TEST_CASE(tcs, do_user_file__missing_syntax);
    ATF_ADD_TEST_CASE(tcs, do_user_file__missing_file);

    ATF_ADD_TEST_CASE(tcs, get_syntax__ok);
    ATF_ADD_TEST_CASE(tcs, get_syntax__no_table);
    ATF_ADD_TEST_CASE(tcs, get_syntax__not_defined);
    ATF_ADD_TEST_CASE(tcs, get_syntax__bad_format);
    ATF_ADD_TEST_CASE(tcs, get_syntax__bad_version);

    ATF_ADD_TEST_CASE(tcs, init__ok);
}
