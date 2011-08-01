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
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/lua/exceptions.hpp"
#include "utils/lua/operations.hpp"
#include "utils/lua/wrap.hpp"

namespace fs = utils::fs;
namespace lua = utils::lua;
namespace user_files = engine::user_files;


ATF_TEST_CASE_WITHOUT_HEAD(empty);
ATF_TEST_CASE_BODY(empty)
{
    std::ofstream output("test.lua");
    ATF_REQUIRE(output);
    output << "syntax('kyuafile', 1)\n";
    output.close();

    lua::state state;
    user_files::do_user_file(state, fs::path("test.lua"));
    lua::do_string(state, "assert(table.maxn(kyuafile.TEST_PROGRAMS) == 0)");
}


ATF_TEST_CASE_WITHOUT_HEAD(some_atf_test_programs__ok);
ATF_TEST_CASE_BODY(some_atf_test_programs__ok)
{
    std::ofstream output("test.lua");
    ATF_REQUIRE(output);
    output << "syntax('kyuafile', 1)\n";
    output << "test_suite('the-default')\n";
    output << "atf_test_program{name='test1'}\n";
    output << "atf_test_program{name='test3', test_suite='overriden'}\n";
    output << "atf_test_program{name='test2'}\n";
    output.close();

    lua::state state;
    user_files::do_user_file(state, fs::path("test.lua"));
    lua::do_string(state, "assert(table.maxn(kyuafile.TEST_PROGRAMS) == 3)");
    lua::do_string(state, "assert(kyuafile.TEST_PROGRAMS[1].name == 'test1')");
    lua::do_string(state, "assert(kyuafile.TEST_PROGRAMS[1].interface == "
                   "'atf')");
    lua::do_string(state, "assert(kyuafile.TEST_PROGRAMS[1].test_suite == "
                   "'the-default')");
    lua::do_string(state, "assert(kyuafile.TEST_PROGRAMS[2].name == 'test3')");
    lua::do_string(state, "assert(kyuafile.TEST_PROGRAMS[2].interface == "
                   "'atf')");
    lua::do_string(state, "assert(kyuafile.TEST_PROGRAMS[2].test_suite == "
                   "'overriden')");
    lua::do_string(state, "assert(kyuafile.TEST_PROGRAMS[3].name == 'test2')");
    lua::do_string(state, "assert(kyuafile.TEST_PROGRAMS[3].interface == "
                   "'atf')");
    lua::do_string(state, "assert(kyuafile.TEST_PROGRAMS[3].test_suite == "
                   "'the-default')");
}


ATF_TEST_CASE_WITHOUT_HEAD(some_atf_test_programs__fail);
ATF_TEST_CASE_BODY(some_atf_test_programs__fail)
{
    std::ofstream output("test.lua");
    ATF_REQUIRE(output);
    output << "syntax('kyuafile', 1)\n";
    output << "test_suite('the-default')\n";
    output << "atf_test_program{name='test1'}\n";
    output << "atf_test_program{name='/a/foo'}\n";
    output.close();

    lua::state state;
    ATF_REQUIRE_THROW_RE(lua::error, "'/a/foo'.*path components",
                         user_files::do_user_file(state, fs::path("test.lua")));
}


ATF_TEST_CASE_WITHOUT_HEAD(some_plain_test_programs__ok);
ATF_TEST_CASE_BODY(some_plain_test_programs__ok)
{
    std::ofstream output("test.lua");
    ATF_REQUIRE(output);
    output << "syntax('kyuafile', 1)\n";
    output << "test_suite('the-default')\n";
    output << "plain_test_program{name='test2'}\n";
    output << "plain_test_program{name='test1', test_suite='overriden'}\n";
    output.close();

    lua::state state;
    user_files::do_user_file(state, fs::path("test.lua"));
    lua::do_string(state, "assert(table.maxn(kyuafile.TEST_PROGRAMS) == 2)");
    lua::do_string(state, "assert(kyuafile.TEST_PROGRAMS[1].name == 'test2')");
    lua::do_string(state, "assert(kyuafile.TEST_PROGRAMS[1].interface == "
                   "'plain')");
    lua::do_string(state, "assert(kyuafile.TEST_PROGRAMS[1].test_suite == "
                   "'the-default')");
    lua::do_string(state, "assert(kyuafile.TEST_PROGRAMS[2].name == 'test1')");
    lua::do_string(state, "assert(kyuafile.TEST_PROGRAMS[2].interface == "
                   "'plain')");
    lua::do_string(state, "assert(kyuafile.TEST_PROGRAMS[2].test_suite == "
                   "'overriden')");
}


ATF_TEST_CASE_WITHOUT_HEAD(some_plain_test_programs__fail);
ATF_TEST_CASE_BODY(some_plain_test_programs__fail)
{
    std::ofstream output("test.lua");
    ATF_REQUIRE(output);
    output << "syntax('kyuafile', 1)\n";
    output << "plain_test_program{name='test1', test_suite='a'}\n";
    output << "plain_test_program{name='/a/foo', test_suite='b'}\n";
    output.close();

    lua::state state;
    ATF_REQUIRE_THROW_RE(lua::error, "'/a/foo'.*path components",
                         user_files::do_user_file(state, fs::path("test.lua")));
}


ATF_TEST_CASE_WITHOUT_HEAD(include_absolute);
ATF_TEST_CASE_BODY(include_absolute)
{
    {
        std::ofstream output("main.lua");
        ATF_REQUIRE(output);
        output << "syntax('kyuafile', 1)\n";
        output << "test_suite('top')\n";
        output << "include('" << (fs::current_path() / "dir/second.lua")
               << "')\n";
        output.close();
    }

    lua::state state;
    ATF_REQUIRE_THROW_RE(lua::error, "second.lua'.*absolute path",
                         user_files::do_user_file(state, fs::path("main.lua")));
}


ATF_TEST_CASE_WITHOUT_HEAD(include_nested);
ATF_TEST_CASE_BODY(include_nested)
{
    {
        std::ofstream output("root.lua");
        ATF_REQUIRE(output);
        output << "syntax('kyuafile', 1)\n";
        output << "test_suite('foo')\n";
        output << "atf_test_program{name='test1'}\n";
        output << "atf_test_program{name='test2'}\n";
        output << "include('dir/test.lua')\n";
        output.close();
    }

    {
        fs::mkdir(fs::path("dir"), 0755);
        std::ofstream output("dir/test.lua");
        ATF_REQUIRE(output);
        output << "syntax('kyuafile', 1)\n";
        output << "atf_test_program{name='test1', test_suite='bar'}\n";
        output << "include('foo/test.lua')\n";
        output.close();
    }

    {
        fs::mkdir(fs::path("dir/foo"), 0755);
        std::ofstream output("dir/foo/test.lua");
        ATF_REQUIRE(output);
        output << "syntax('kyuafile', 1)\n";
        output << "atf_test_program{name='bar', test_suite='baz'}\n";
        output << "atf_test_program{name='baz', test_suite='baz'}\n";
        output.close();
    }

    lua::state state;
    user_files::do_user_file(state, fs::path("root.lua"));
    lua::do_string(state, "assert(table.maxn(kyuafile.TEST_PROGRAMS) == 5)");
    lua::do_string(state,
                   "assert(kyuafile.TEST_PROGRAMS[1].name == 'test1')");
    lua::do_string(state,
                   "assert(kyuafile.TEST_PROGRAMS[1].test_suite == 'foo')");
    lua::do_string(state,
                   "assert(kyuafile.TEST_PROGRAMS[2].name == 'test2')");
    lua::do_string(state,
                   "assert(kyuafile.TEST_PROGRAMS[2].test_suite == 'foo')");
    lua::do_string(state,
                   "assert(kyuafile.TEST_PROGRAMS[3].name == 'dir/test1')");
    lua::do_string(state,
                   "assert(kyuafile.TEST_PROGRAMS[3].test_suite == 'bar')");
    lua::do_string(state,
                   "assert(kyuafile.TEST_PROGRAMS[4].name == 'dir/foo/bar')");
    lua::do_string(state,
                   "assert(kyuafile.TEST_PROGRAMS[4].test_suite == 'baz')");
    lua::do_string(state,
                   "assert(kyuafile.TEST_PROGRAMS[5].name == 'dir/foo/baz')");
    lua::do_string(state,
                   "assert(kyuafile.TEST_PROGRAMS[5].test_suite == 'baz')");
}


ATF_TEST_CASE_WITHOUT_HEAD(include_same_dir);
ATF_TEST_CASE_BODY(include_same_dir)
{
    {
        std::ofstream output("main.lua");
        ATF_REQUIRE(output);
        output << "syntax('kyuafile', 1)\n";
        output << "test_suite('abcd')\n";
        output << "atf_test_program{name='test1'}\n";
        output << "atf_test_program{name='test2'}\n";
        output << "include('second.lua')\n";
        output.close();
    }

    {
        std::ofstream output("second.lua");
        ATF_REQUIRE(output);
        output << "syntax('kyuafile', 1)\n";
        output << "test_suite('efgh')\n";
        output << "atf_test_program{name='test12'}\n";
        output.close();
    }

    lua::state state;
    user_files::do_user_file(state, fs::path("main.lua"));
    lua::do_string(state, "assert(table.maxn(kyuafile.TEST_PROGRAMS) == 3)");
    lua::do_string(state, "assert(kyuafile.TEST_PROGRAMS[1].name == 'test1')");
    lua::do_string(state,
                   "assert(kyuafile.TEST_PROGRAMS[1].test_suite == 'abcd')");
    lua::do_string(state, "assert(kyuafile.TEST_PROGRAMS[2].name == 'test2')");
    lua::do_string(state,
                   "assert(kyuafile.TEST_PROGRAMS[2].test_suite == 'abcd')");
    lua::do_string(state, "assert(kyuafile.TEST_PROGRAMS[3].name == 'test12')");
    lua::do_string(state,
                   "assert(kyuafile.TEST_PROGRAMS[3].test_suite == 'efgh')");
}


ATF_TEST_CASE_WITHOUT_HEAD(test_suite__ok);
ATF_TEST_CASE_BODY(test_suite__ok)
{
    std::ofstream output("test.lua");
    ATF_REQUIRE(output);
    output << "syntax('kyuafile', 1)\n";
    output << "test_suite('the-test-suite')\n";
    output.close();

    lua::state state;
    user_files::do_user_file(state, fs::path("test.lua"));
    lua::do_string(state, "assert(kyuafile.TEST_SUITE == 'the-test-suite')");
}


ATF_TEST_CASE_WITHOUT_HEAD(test_suite__twice);
ATF_TEST_CASE_BODY(test_suite__twice)
{
    std::ofstream output("test.lua");
    ATF_REQUIRE(output);
    output << "syntax('kyuafile', 1)\n";
    output << "test_suite('the-test-suite-1')\n";
    output << "test_suite('the-test-suite-2')\n";
    output.close();

    lua::state state;
    ATF_REQUIRE_THROW_RE(lua::error, "cannot call test_suite twice",
                         user_files::do_user_file(state, fs::path("test.lua")));
    lua::do_string(state, "assert(kyuafile.TEST_SUITE == 'the-test-suite-1')");
}


ATF_TEST_CASE_WITHOUT_HEAD(test_suite__not_recursive);
ATF_TEST_CASE_BODY(test_suite__not_recursive)
{
    {
        std::ofstream output("main.lua");
        ATF_REQUIRE(output);
        output << "syntax('kyuafile', 1)\n";
        output << "test_suite('abcd')\n";
        output << "atf_test_program{name='test1'}\n";
        output << "include('second.lua')\n";
        output.close();
    }

    {
        std::ofstream output("second.lua");
        ATF_REQUIRE(output);
        output << "syntax('kyuafile', 1)\n";
        output << "atf_test_program{name='test12'}\n";
        output.close();
    }

    lua::state state;
    ATF_REQUIRE_THROW_RE(lua::error, "no test suite.*test program 'test12'",
                         user_files::do_user_file(state, fs::path("main.lua")));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, empty);
    ATF_ADD_TEST_CASE(tcs, some_atf_test_programs__ok);
    ATF_ADD_TEST_CASE(tcs, some_atf_test_programs__fail);
    ATF_ADD_TEST_CASE(tcs, some_plain_test_programs__ok);
    ATF_ADD_TEST_CASE(tcs, some_plain_test_programs__fail);
    ATF_ADD_TEST_CASE(tcs, include_absolute);
    ATF_ADD_TEST_CASE(tcs, include_nested);
    ATF_ADD_TEST_CASE(tcs, include_same_dir);
    ATF_ADD_TEST_CASE(tcs, test_suite__ok);
    ATF_ADD_TEST_CASE(tcs, test_suite__twice);
    ATF_ADD_TEST_CASE(tcs, test_suite__not_recursive);
}
