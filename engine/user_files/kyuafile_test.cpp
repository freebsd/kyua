// Copyright 2010, 2011 Google Inc.
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

#include <fstream>

#include <atf-c++.hpp>

#include "engine/exceptions.hpp"
#include "engine/user_files/kyuafile.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/fs/operations.hpp"
#include "utils/lua/operations.hpp"
#include "utils/lua/test_utils.hpp"
#include "utils/lua/wrap.ipp"

namespace cmdline = utils::cmdline;
namespace fs = utils::fs;
namespace lua = utils::lua;
namespace user_files = engine::user_files;


ATF_TEST_CASE_WITHOUT_HEAD(adjust_binary_path__absolute);
ATF_TEST_CASE_BODY(adjust_binary_path__absolute)
{
    ATF_REQUIRE_EQ(fs::path("/foo/bar"),
                   user_files::detail::adjust_binary_path(
                       fs::path("/foo/bar"), fs::path("/dont/care")));
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust_binary_path__current_directory);
ATF_TEST_CASE_BODY(adjust_binary_path__current_directory)
{
    ATF_REQUIRE_EQ(fs::path("test_program"),
                   user_files::detail::adjust_binary_path(
                       fs::path("test_program"), fs::path(".")));
}


ATF_TEST_CASE_WITHOUT_HEAD(adjust_binary_path__relative);
ATF_TEST_CASE_BODY(adjust_binary_path__relative)
{
    ATF_REQUIRE_EQ(fs::path("foo/bar/test_program"),
                   user_files::detail::adjust_binary_path(
                       fs::path("test_program"), fs::path("foo/bar")));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_program__ok);
ATF_TEST_CASE_BODY(get_test_program__ok)
{
    lua::state state;
    stack_balance_checker checker(state);

    lua::do_string(state, "return {name='the-name', "
                   "test_suite='the-suite'}", 1);

    const user_files::test_program program =
        user_files::detail::get_test_program(state, fs::path("root/directory"));
    ATF_REQUIRE_EQ(fs::path("root/directory/the-name"), program.binary_path);
    ATF_REQUIRE_EQ("the-suite", program.test_suite_name);

    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_program__bad_name);
ATF_TEST_CASE_BODY(get_test_program__bad_name)
{
    lua::state state;
    stack_balance_checker checker(state);

    lua::do_string(state, "return {name={}, test_suite='the-suite'}", 1);

    ATF_REQUIRE_THROW_RE(engine::error, "non-string.*test program$",
                         user_files::detail::get_test_program(
                             state, fs::path(".")));

    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_program__bad_test_suite);
ATF_TEST_CASE_BODY(get_test_program__bad_test_suite)
{
    lua::state state;
    stack_balance_checker checker(state);

    lua::do_string(state, "return {name='foo', test_suite={}}", 1);

    ATF_REQUIRE_THROW_RE(engine::error, "non-string.*test suite.*'bar/foo'",
                         user_files::detail::get_test_program(
                             state, fs::path("bar")));

    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_programs__empty);
ATF_TEST_CASE_BODY(get_test_programs__empty)
{
    lua::state state;
    stack_balance_checker checker(state);

    lua::do_string(state, "t = {}");

    const user_files::test_programs_vector programs =
        user_files::detail::get_test_programs(state, "t", fs::path("a/b"));
    ATF_REQUIRE(programs.empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_programs__some);
ATF_TEST_CASE_BODY(get_test_programs__some)
{
    lua::state state;
    stack_balance_checker checker(state);

    lua::do_string(state, "t = {}; t[1] = {name='a', test_suite='b'}; "
                   "t[2] = {name='c/d', test_suite='e'}");

    const user_files::test_programs_vector programs =
        user_files::detail::get_test_programs(state, "t", fs::path("root"));
    ATF_REQUIRE_EQ(2, programs.size());

    ATF_REQUIRE_EQ(fs::path("root/a"), programs[0].binary_path);
    ATF_REQUIRE_EQ("b", programs[0].test_suite_name);

    ATF_REQUIRE_EQ(fs::path("root/c/d"), programs[1].binary_path);
    ATF_REQUIRE_EQ("e", programs[1].test_suite_name);
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_programs__invalid);
ATF_TEST_CASE_BODY(get_test_programs__invalid)
{
    lua::state state;
    stack_balance_checker checker(state);

    lua::do_string(state, "t = 'foo'");

    ATF_REQUIRE_THROW_RE(engine::error, "'t' is not a table",
                         user_files::detail::get_test_programs(
                             state, "t", fs::path(".")));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_programs__bad_value);
ATF_TEST_CASE_BODY(get_test_programs__bad_value)
{
    lua::state state;
    stack_balance_checker checker(state);

    lua::do_string(state, "t = {}; t[1] = 'foo'");

    ATF_REQUIRE_THROW_RE(engine::error, "Expected table in 't'",
                         user_files::detail::get_test_programs(
                             state, "t", fs::path(".")));
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__current_directory);
ATF_TEST_CASE_BODY(kyuafile__load__current_directory)
{
    {
        std::ofstream file("config");
        file << "syntax('kyuafile', 1)\n";
        file << "AtfTestProgram {name='one', test_suite='first'}\n";
        file << "include('dir/config')\n";
        file.close();
    }

    {
        fs::mkdir(fs::path("dir"), 0755);
        std::ofstream file("dir/config");
        file << "syntax('kyuafile', 1)\n";
        file << "test_suite('second')\n";
        file << "AtfTestProgram {name='two'}\n";
        file.close();
    }

    const user_files::kyuafile suite = user_files::kyuafile::load(
        fs::path("config"));
    ATF_REQUIRE_EQ(2, suite.test_programs().size());
    ATF_REQUIRE_EQ(fs::path("one"), suite.test_programs()[0].binary_path);
    ATF_REQUIRE_EQ("first", suite.test_programs()[0].test_suite_name);
    ATF_REQUIRE_EQ(fs::path("dir/two"), suite.test_programs()[1].binary_path);
    ATF_REQUIRE_EQ("second", suite.test_programs()[1].test_suite_name);
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__other_directory);
ATF_TEST_CASE_BODY(kyuafile__load__other_directory)
{
    {
        fs::mkdir(fs::path("root"), 0755);
        std::ofstream file("root/config");
        file << "syntax('kyuafile', 1)\n";
        file << "test_suite('abc')\n";
        file << "AtfTestProgram {name='one'}\n";
        file << "AtfTestProgram {name='/a/b/two'}\n";
        file << "include('dir/config')\n";
        file.close();
    }

    {
        fs::mkdir(fs::path("root/dir"), 0755);
        std::ofstream file("root/dir/config");
        file << "syntax('kyuafile', 1)\n";
        file << "AtfTestProgram {name='three', test_suite='def'}\n";
        file << "AtfTestProgram {name='/four', test_suite='def'}\n";
        file.close();
    }

    const user_files::kyuafile suite = user_files::kyuafile::load(
        fs::path("root/config"));
    ATF_REQUIRE_EQ(4, suite.test_programs().size());
    ATF_REQUIRE_EQ(fs::path("root/one"), suite.test_programs()[0].binary_path);
    ATF_REQUIRE_EQ("abc", suite.test_programs()[0].test_suite_name);
    ATF_REQUIRE_EQ(fs::path("/a/b/two"), suite.test_programs()[1].binary_path);
    ATF_REQUIRE_EQ("abc", suite.test_programs()[1].test_suite_name);
    ATF_REQUIRE_EQ(fs::path("root/dir/three"),
                   suite.test_programs()[2].binary_path);
    ATF_REQUIRE_EQ("def", suite.test_programs()[2].test_suite_name);
    ATF_REQUIRE_EQ(fs::path("/four"), suite.test_programs()[3].binary_path);
    ATF_REQUIRE_EQ("def", suite.test_programs()[3].test_suite_name);
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__lua_error);
ATF_TEST_CASE_BODY(kyuafile__load__lua_error)
{
    std::ofstream file("config");
    file << "this syntax is invalid\n";
    file.close();

    ATF_REQUIRE_THROW(engine::error, user_files::kyuafile::load(
        fs::path("config")));
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__bad_syntax__format);
ATF_TEST_CASE_BODY(kyuafile__load__bad_syntax__format)
{
    std::ofstream file("config");
    file << "syntax('kyuafile', 1)\n";
    file << "init.get_syntax().format = 'foo'\n";
    file.close();

    ATF_REQUIRE_THROW_RE(engine::error, "Unexpected file format 'foo'",
                         user_files::kyuafile::load(fs::path("config")));
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__bad_syntax__version);
ATF_TEST_CASE_BODY(kyuafile__load__bad_syntax__version)
{
    std::ofstream file("config");
    file << "syntax('kyuafile', 1)\n";
    file << "init.get_syntax().version = 123\n";
    file.close();

    ATF_REQUIRE_THROW_RE(engine::error, "Unexpected file version '123'",
                         user_files::kyuafile::load(fs::path("config")));
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__missing_file);
ATF_TEST_CASE_BODY(kyuafile__load__missing_file)
{
    ATF_REQUIRE_THROW_RE(engine::error, "Load failed",
                         user_files::kyuafile::load(fs::path("missing")));
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__from_arguments__none);
ATF_TEST_CASE_BODY(kyuafile__from_arguments__none)
{
    const user_files::kyuafile suite = user_files::kyuafile::from_arguments(
        cmdline::args_vector());
    ATF_REQUIRE_EQ(0, suite.test_programs().size());
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__from_arguments__some);
ATF_TEST_CASE_BODY(kyuafile__from_arguments__some)
{
    cmdline::args_vector args;
    args.push_back("a/b/c");
    args.push_back("foo/bar");
    const user_files::kyuafile suite = user_files::kyuafile::from_arguments(
        args);
    ATF_REQUIRE_EQ(2, suite.test_programs().size());
    ATF_REQUIRE_EQ(fs::path("a/b/c"), suite.test_programs()[0].binary_path);
    ATF_REQUIRE_EQ("__undefined__", suite.test_programs()[0].test_suite_name);
    ATF_REQUIRE_EQ(fs::path("foo/bar"), suite.test_programs()[1].binary_path);
    ATF_REQUIRE_EQ("__undefined__", suite.test_programs()[1].test_suite_name);
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__from_arguments__with_test_case);
ATF_TEST_CASE_BODY(kyuafile__from_arguments__with_test_case)
{
    cmdline::args_vector args;
    args.push_back("foo/bar:test_case");
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "not implemented",
                         user_files::kyuafile::from_arguments(args));
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__from_arguments__invalid_path);
ATF_TEST_CASE_BODY(kyuafile__from_arguments__invalid_path)
{
    cmdline::args_vector args;
    args.push_back("");
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "Invalid path",
                         user_files::kyuafile::from_arguments(args));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, adjust_binary_path__absolute);
    ATF_ADD_TEST_CASE(tcs, adjust_binary_path__current_directory);
    ATF_ADD_TEST_CASE(tcs, adjust_binary_path__relative);

    ATF_ADD_TEST_CASE(tcs, get_test_program__ok);
    ATF_ADD_TEST_CASE(tcs, get_test_program__bad_name);
    ATF_ADD_TEST_CASE(tcs, get_test_program__bad_test_suite);

    ATF_ADD_TEST_CASE(tcs, get_test_programs__empty);
    ATF_ADD_TEST_CASE(tcs, get_test_programs__some);
    ATF_ADD_TEST_CASE(tcs, get_test_programs__invalid);
    ATF_ADD_TEST_CASE(tcs, get_test_programs__bad_value);

    ATF_ADD_TEST_CASE(tcs, kyuafile__load__current_directory);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__other_directory);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__lua_error);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__bad_syntax__format);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__bad_syntax__version);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__missing_file);

    ATF_ADD_TEST_CASE(tcs, kyuafile__from_arguments__none);
    ATF_ADD_TEST_CASE(tcs, kyuafile__from_arguments__some);
    ATF_ADD_TEST_CASE(tcs, kyuafile__from_arguments__with_test_case);
    ATF_ADD_TEST_CASE(tcs, kyuafile__from_arguments__invalid_path);
}
