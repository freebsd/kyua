// Copyright 2010 Google Inc.
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

#include "engine/user_files/kyuafile.hpp"

#include <fstream>
#include <stdexcept>
#include <typeinfo>

#include <atf-c++.hpp>
#include <lutok/operations.hpp>
#include <lutok/state.ipp>
#include <lutok/test_utils.hpp>

#include "engine/atf_iface/test_program.hpp"
#include "engine/plain_iface/test_program.hpp"
#include "engine/user_files/exceptions.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/test_utils.hpp"

namespace atf_iface = engine::atf_iface;
namespace fs = utils::fs;
namespace plain_iface = engine::plain_iface;
namespace user_files = engine::user_files;


/// Checks test program name validation in get_test_program().
///
/// \param interface The name of the test interface to check.
static void
bad_name_test(const char* interface)
{
    lutok::state state;
    stack_balance_checker checker(state);

    lutok::do_string(state, F("return {name={}, interface='%s', "
                              "test_suite='the-suite'}") % interface, 1);

    ATF_REQUIRE_THROW_RE(std::runtime_error, "non-string.*test program$",
                         user_files::detail::get_test_program(
                             state, fs::path(".")));

    state.pop(1);
}


/// Checks test suite name validation in get_test_program().
///
/// \param interface The name of the test interface to check.
static void
bad_test_suite_test(const char* interface)
{
    lutok::state state;
    stack_balance_checker checker(state);

    lutok::do_string(state, F("return {name='foo', interface='%s', "
                              "test_suite={}}") % interface, 1);
    fs::mkdir(fs::path("bar"), 0755);
    utils::create_file(fs::path("bar/foo"));

    ATF_REQUIRE_THROW_RE(std::runtime_error,
                         "non-string.*test suite.*'foo'",
                         user_files::detail::get_test_program(
                             state, fs::path("bar")));

    state.pop(1);
}


/// Checks test program existance validation in get_test_program().
///
/// \param interface The name of the test interface to check.
static void
missing_binary_test(const char* interface)
{
    lutok::state state;
    stack_balance_checker checker(state);

    utils::create_file(fs::path("i-exist"));
    lutok::do_string(state, F("return {name='i-exist', interface='%s', "
                              "test_suite='the-suite'}") % interface, 1);

    ATF_REQUIRE_THROW_RE(std::runtime_error,
                         "Non-existent.*'i-exist'",
                         user_files::detail::get_test_program(
                             state, fs::path("root")));

    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_program__atf__ok);
ATF_TEST_CASE_BODY(get_test_program__atf__ok)
{
    lutok::state state;
    stack_balance_checker checker(state);

    lutok::do_string(state, "return {name='the-name', interface='atf', "
                     "test_suite='the-suite'}", 1);

    fs::mkdir(fs::path("root"), 0755);
    fs::mkdir(fs::path("root/directory"), 0755);
    utils::create_file(fs::path("root/directory/the-name"));

    const engine::test_program_ptr program =
        user_files::detail::get_test_program(state, fs::path("root/directory"));
    ATF_REQUIRE(typeid(atf_iface::test_program) == typeid(*program));
    ATF_REQUIRE_EQ(fs::path("the-name"), program->relative_path());
    ATF_REQUIRE_EQ("the-suite", program->test_suite_name());

    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_program__atf__bad_name);
ATF_TEST_CASE_BODY(get_test_program__atf__bad_name)
{
    bad_name_test("atf");
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_program__atf__bad_test_suite);
ATF_TEST_CASE_BODY(get_test_program__atf__bad_test_suite)
{
    bad_test_suite_test("atf");
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_program__atf__missing_binary);
ATF_TEST_CASE_BODY(get_test_program__atf__missing_binary)
{
    missing_binary_test("atf");
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_program__plain__ok);
ATF_TEST_CASE_BODY(get_test_program__plain__ok)
{
    lutok::state state;
    stack_balance_checker checker(state);

    lutok::do_string(state, "return {name='the-name', interface='plain', "
                     "test_suite='the-suite'}", 1);

    fs::mkdir(fs::path("root"), 0755);
    fs::mkdir(fs::path("root/directory"), 0755);
    utils::create_file(fs::path("root/directory/the-name"));

    const engine::test_program_ptr program =
        user_files::detail::get_test_program(state, fs::path("root/directory"));
    ATF_REQUIRE(typeid(plain_iface::test_program) == typeid(*program));
    ATF_REQUIRE_EQ(fs::path("the-name"), program->relative_path());
    ATF_REQUIRE_EQ("the-suite", program->test_suite_name());

    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_program__plain__bad_name);
ATF_TEST_CASE_BODY(get_test_program__plain__bad_name)
{
    bad_name_test("plain");
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_program__plain__bad_test_suite);
ATF_TEST_CASE_BODY(get_test_program__plain__bad_test_suite)
{
    bad_test_suite_test("plain");
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_program__plain__missing_binary);
ATF_TEST_CASE_BODY(get_test_program__plain__missing_binary)
{
    missing_binary_test("plain");
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_program__missing_interface);
ATF_TEST_CASE_BODY(get_test_program__missing_interface)
{
    lutok::state state;
    stack_balance_checker checker(state);

    lutok::do_string(state, "return {name='the-name'}", 1);

    ATF_REQUIRE_THROW_RE(std::runtime_error, "Missing test case interface",
                         user_files::detail::get_test_program(
                             state, fs::path("root")));

    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_program__unsupported_interface);
ATF_TEST_CASE_BODY(get_test_program__unsupported_interface)
{
    lutok::state state;
    stack_balance_checker checker(state);

    lutok::do_string(state, "return {name='the-name', interface='foo'}", 1);

    ATF_REQUIRE_THROW_RE(std::runtime_error,
                         "Unsupported test interface 'foo'",
                         user_files::detail::get_test_program(
                             state, fs::path("root")));

    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_programs__empty);
ATF_TEST_CASE_BODY(get_test_programs__empty)
{
    lutok::state state;
    stack_balance_checker checker(state);

    lutok::do_string(state, "t = {}");

    const engine::test_programs_vector programs =
        user_files::detail::get_test_programs(state, "t", fs::path("a/b"));
    ATF_REQUIRE(programs.empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_programs__some);
ATF_TEST_CASE_BODY(get_test_programs__some)
{
    lutok::state state;
    stack_balance_checker checker(state);

    lutok::do_string(
        state, "t = {}; t[1] = {name='a', interface='atf', test_suite='b'};"
        "t[2] = {name='c/d', interface='atf', test_suite='e'}");

    fs::mkdir(fs::path("root"), 0755);
    utils::create_file(fs::path("root/a"));
    fs::mkdir(fs::path("root/c"), 0755);
    utils::create_file(fs::path("root/c/d"));

    const engine::test_programs_vector programs =
        user_files::detail::get_test_programs(state, "t", fs::path("root"));
    ATF_REQUIRE_EQ(2, programs.size());

    ATF_REQUIRE_EQ(fs::path("a"), programs[0]->relative_path());
    ATF_REQUIRE_EQ("b", programs[0]->test_suite_name());

    ATF_REQUIRE_EQ(fs::path("c/d"), programs[1]->relative_path());
    ATF_REQUIRE_EQ("e", programs[1]->test_suite_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_programs__invalid);
ATF_TEST_CASE_BODY(get_test_programs__invalid)
{
    lutok::state state;
    stack_balance_checker checker(state);

    lutok::do_string(state, "t = 'foo'");

    ATF_REQUIRE_THROW_RE(std::runtime_error, "'t' is not a table",
                         user_files::detail::get_test_programs(
                             state, "t", fs::path(".")));
}


ATF_TEST_CASE_WITHOUT_HEAD(get_test_programs__bad_value);
ATF_TEST_CASE_BODY(get_test_programs__bad_value)
{
    lutok::state state;
    stack_balance_checker checker(state);

    lutok::do_string(state, "t = {}; t[1] = 'foo'");

    ATF_REQUIRE_THROW_RE(std::runtime_error, "Expected table in 't'",
                         user_files::detail::get_test_programs(
                             state, "t", fs::path(".")));
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__integration);
ATF_TEST_CASE_BODY(kyuafile__load__integration)
{
    {
        std::ofstream file("config");
        file << "syntax('kyuafile', 1)\n";
        file << "test_suite('one-suite')\n";
        file << "atf_test_program{name='1st'}\n";
        file << "atf_test_program{name='2nd', test_suite='first'}\n";
        file << "plain_test_program{name='3rd'}\n";
        file << "plain_test_program{name='4th', test_suite='second'}\n";
        file << "include('dir/config')\n";
        file.close();
    }

    {
        fs::mkdir(fs::path("dir"), 0755);
        std::ofstream file("dir/config");
        file << "syntax('kyuafile', 1)\n";
        file << "atf_test_program{name='1st', test_suite='other-suite'}\n";
        file.close();
    }

    utils::create_file(fs::path("1st"));
    utils::create_file(fs::path("2nd"));
    utils::create_file(fs::path("3rd"));
    utils::create_file(fs::path("4th"));
    utils::create_file(fs::path("dir/1st"));

    const user_files::kyuafile suite = user_files::kyuafile::load(
        fs::path("config"));
    ATF_REQUIRE_EQ(fs::path("."), suite.root());
    ATF_REQUIRE_EQ(5, suite.test_programs().size());

    ATF_REQUIRE(typeid(atf_iface::test_program) ==
                typeid(*suite.test_programs()[0]));
    ATF_REQUIRE_EQ(fs::path("1st"), suite.test_programs()[0]->relative_path());
    ATF_REQUIRE_EQ("one-suite", suite.test_programs()[0]->test_suite_name());

    ATF_REQUIRE(typeid(atf_iface::test_program) ==
                typeid(*suite.test_programs()[1]));
    ATF_REQUIRE_EQ(fs::path("2nd"), suite.test_programs()[1]->relative_path());
    ATF_REQUIRE_EQ("first", suite.test_programs()[1]->test_suite_name());

    ATF_REQUIRE(typeid(plain_iface::test_program) ==
                typeid(*suite.test_programs()[2]));
    ATF_REQUIRE_EQ(fs::path("3rd"), suite.test_programs()[2]->relative_path());
    ATF_REQUIRE_EQ("one-suite", suite.test_programs()[2]->test_suite_name());

    ATF_REQUIRE(typeid(plain_iface::test_program) ==
                typeid(*suite.test_programs()[3]));
    ATF_REQUIRE_EQ(fs::path("4th"), suite.test_programs()[3]->relative_path());
    ATF_REQUIRE_EQ("second", suite.test_programs()[3]->test_suite_name());

    ATF_REQUIRE(typeid(atf_iface::test_program) ==
                typeid(*suite.test_programs()[4]));
    ATF_REQUIRE_EQ(fs::path("dir/1st"),
                   suite.test_programs()[4]->relative_path());
    ATF_REQUIRE_EQ("other-suite", suite.test_programs()[4]->test_suite_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__current_directory);
ATF_TEST_CASE_BODY(kyuafile__load__current_directory)
{
    {
        std::ofstream file("config");
        file << "syntax('kyuafile', 1)\n";
        file << "atf_test_program{name='one', test_suite='first'}\n";
        file << "include('dir/config')\n";
        file.close();
    }

    {
        fs::mkdir(fs::path("dir"), 0755);
        std::ofstream file("dir/config");
        file << "syntax('kyuafile', 1)\n";
        file << "test_suite('second')\n";
        file << "atf_test_program{name='two'}\n";
        file.close();
    }

    utils::create_file(fs::path("one"));
    utils::create_file(fs::path("dir/two"));

    const user_files::kyuafile suite = user_files::kyuafile::load(
        fs::path("config"));
    ATF_REQUIRE_EQ(fs::path("."), suite.root());
    ATF_REQUIRE_EQ(2, suite.test_programs().size());
    ATF_REQUIRE_EQ(fs::path("one"), suite.test_programs()[0]->relative_path());
    ATF_REQUIRE_EQ("first", suite.test_programs()[0]->test_suite_name());
    ATF_REQUIRE_EQ(fs::path("dir/two"),
                   suite.test_programs()[1]->relative_path());
    ATF_REQUIRE_EQ("second", suite.test_programs()[1]->test_suite_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__other_directory);
ATF_TEST_CASE_BODY(kyuafile__load__other_directory)
{
    {
        fs::mkdir(fs::path("root"), 0755);
        std::ofstream file("root/config");
        file << "syntax('kyuafile', 1)\n";
        file << "test_suite('abc')\n";
        file << "atf_test_program{name='one'}\n";
        file << "include('dir/config')\n";
        file.close();
    }

    {
        fs::mkdir(fs::path("root/dir"), 0755);
        std::ofstream file("root/dir/config");
        file << "syntax('kyuafile', 1)\n";
        file << "test_suite('foo')\n";
        file << "atf_test_program{name='two', test_suite='def'}\n";
        file << "atf_test_program{name='three'}\n";
        file.close();
    }

    utils::create_file(fs::path("root/one"));
    utils::create_file(fs::path("root/dir/two"));
    utils::create_file(fs::path("root/dir/three"));

    const user_files::kyuafile suite = user_files::kyuafile::load(
        fs::path("root/config"));
    ATF_REQUIRE_EQ(fs::path("root"), suite.root());
    ATF_REQUIRE_EQ(3, suite.test_programs().size());
    ATF_REQUIRE_EQ(fs::path("one"), suite.test_programs()[0]->relative_path());
    ATF_REQUIRE_EQ("abc", suite.test_programs()[0]->test_suite_name());
    ATF_REQUIRE_EQ(fs::path("dir/two"),
                   suite.test_programs()[1]->relative_path());
    ATF_REQUIRE_EQ("def", suite.test_programs()[1]->test_suite_name());
    ATF_REQUIRE_EQ(fs::path("dir/three"),
                   suite.test_programs()[2]->relative_path());
    ATF_REQUIRE_EQ("foo", suite.test_programs()[2]->test_suite_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__test_program_not_basename);
ATF_TEST_CASE_BODY(kyuafile__load__test_program_not_basename)
{
    {
        std::ofstream file("config");
        file << "syntax('kyuafile', 1)\n";
        file << "test_suite('abc')\n";
        file << "atf_test_program{name='one'}\n";
        file << "atf_test_program{name='./ls'}\n";
        file.close();
    }

    ATF_REQUIRE_THROW_RE(user_files::load_error, "./ls.*path components",
                         user_files::kyuafile::load(fs::path("config")));
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__lua_error);
ATF_TEST_CASE_BODY(kyuafile__load__lua_error)
{
    std::ofstream file("config");
    file << "this syntax is invalid\n";
    file.close();

    ATF_REQUIRE_THROW(user_files::load_error, user_files::kyuafile::load(
        fs::path("config")));
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__bad_syntax__format);
ATF_TEST_CASE_BODY(kyuafile__load__bad_syntax__format)
{
    std::ofstream file("config");
    file << "syntax('kyuafile', 1)\n";
    file << "init.get_syntax().format = 'foo'\n";
    file.close();

    ATF_REQUIRE_THROW_RE(user_files::load_error, "Unexpected file format 'foo'",
                         user_files::kyuafile::load(fs::path("config")));
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__bad_syntax__version);
ATF_TEST_CASE_BODY(kyuafile__load__bad_syntax__version)
{
    std::ofstream file("config");
    file << "syntax('kyuafile', 1)\n";
    file << "init.get_syntax().version = 12\n";
    file.close();

    ATF_REQUIRE_THROW_RE(user_files::load_error, "Unexpected file version '12'",
                         user_files::kyuafile::load(fs::path("config")));
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__missing_file);
ATF_TEST_CASE_BODY(kyuafile__load__missing_file)
{
    ATF_REQUIRE_THROW_RE(user_files::load_error, "Load of 'missing' failed",
                         user_files::kyuafile::load(fs::path("missing")));
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__missing_test_program);
ATF_TEST_CASE_BODY(kyuafile__load__missing_test_program)
{
    {
        std::ofstream file("config");
        file << "syntax('kyuafile', 1)\n";
        file << "atf_test_program{name='one', test_suite='first'}\n";
        file << "atf_test_program{name='two', test_suite='first'}\n";
        file.close();
    }

    utils::create_file(fs::path("one"));

    ATF_REQUIRE_THROW_RE(user_files::load_error, "Non-existent.*'two'",
                         user_files::kyuafile::load(fs::path("config")));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, get_test_program__atf__ok);
    ATF_ADD_TEST_CASE(tcs, get_test_program__atf__bad_name);
    ATF_ADD_TEST_CASE(tcs, get_test_program__atf__bad_test_suite);
    ATF_ADD_TEST_CASE(tcs, get_test_program__atf__missing_binary);
    ATF_ADD_TEST_CASE(tcs, get_test_program__plain__ok);
    ATF_ADD_TEST_CASE(tcs, get_test_program__plain__bad_name);
    ATF_ADD_TEST_CASE(tcs, get_test_program__plain__bad_test_suite);
    ATF_ADD_TEST_CASE(tcs, get_test_program__plain__missing_binary);
    ATF_ADD_TEST_CASE(tcs, get_test_program__missing_interface);
    ATF_ADD_TEST_CASE(tcs, get_test_program__unsupported_interface);

    ATF_ADD_TEST_CASE(tcs, get_test_programs__empty);
    ATF_ADD_TEST_CASE(tcs, get_test_programs__some);
    ATF_ADD_TEST_CASE(tcs, get_test_programs__invalid);
    ATF_ADD_TEST_CASE(tcs, get_test_programs__bad_value);

    ATF_ADD_TEST_CASE(tcs, kyuafile__load__integration);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__current_directory);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__other_directory);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__test_program_not_basename);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__lua_error);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__bad_syntax__format);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__bad_syntax__version);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__missing_file);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__missing_test_program);
}
