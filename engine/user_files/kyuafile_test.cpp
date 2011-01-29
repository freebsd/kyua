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

namespace cmdline = utils::cmdline;
namespace fs = utils::fs;
namespace user_files = engine::user_files;


ATF_TEST_CASE_WITHOUT_HEAD(load__current_directory);
ATF_TEST_CASE_BODY(load__current_directory)
{
    {
        std::ofstream file("config");
        file << "syntax('kyuafile', 1)\n";
        file << "AtfTestProgram {name='one'}\n";
        file << "include('dir/config')\n";
        file.close();
    }

    {
        fs::mkdir(fs::path("dir"), 0755);
        std::ofstream file("dir/config");
        file << "syntax('kyuafile', 1)\n";
        file << "AtfTestProgram {name='two'}\n";
        file.close();
    }

    const user_files::kyuafile suite = user_files::kyuafile::load(
        fs::path("config"));
    ATF_REQUIRE_EQ(2, suite.test_programs().size());
    ATF_REQUIRE_EQ(fs::path("one"), suite.test_programs()[0]);
    ATF_REQUIRE_EQ(fs::path("dir/two"), suite.test_programs()[1]);
}


ATF_TEST_CASE_WITHOUT_HEAD(load__other_directory);
ATF_TEST_CASE_BODY(load__other_directory)
{
    {
        fs::mkdir(fs::path("root"), 0755);
        std::ofstream file("root/config");
        file << "syntax('kyuafile', 1)\n";
        file << "AtfTestProgram {name='one'}\n";
        file << "AtfTestProgram {name='/a/b/two'}\n";
        file << "include('dir/config')\n";
        file.close();
    }

    {
        fs::mkdir(fs::path("root/dir"), 0755);
        std::ofstream file("root/dir/config");
        file << "syntax('kyuafile', 1)\n";
        file << "AtfTestProgram {name='three'}\n";
        file << "AtfTestProgram {name='/four'}\n";
        file.close();
    }

    const user_files::kyuafile suite = user_files::kyuafile::load(
        fs::path("root/config"));
    ATF_REQUIRE_EQ(4, suite.test_programs().size());
    ATF_REQUIRE_EQ(fs::path("root/one"), suite.test_programs()[0]);
    ATF_REQUIRE_EQ(fs::path("/a/b/two"), suite.test_programs()[1]);
    ATF_REQUIRE_EQ(fs::path("root/dir/three"), suite.test_programs()[2]);
    ATF_REQUIRE_EQ(fs::path("/four"), suite.test_programs()[3]);
}


ATF_TEST_CASE_WITHOUT_HEAD(load__bad_syntax);
ATF_TEST_CASE_BODY(load__bad_syntax)
{
    std::ofstream file("config");
    file << "syntax('unknown-file-type', 1)\n";
    file.close();

    ATF_REQUIRE_THROW_RE(engine::error, "Load failed.*unknown-file-type",
                         user_files::kyuafile::load(fs::path("config")));
}


ATF_TEST_CASE_WITHOUT_HEAD(load__missing_file);
ATF_TEST_CASE_BODY(load__missing_file)
{
    ATF_REQUIRE_THROW_RE(engine::error, "Load failed",
                         user_files::kyuafile::load(fs::path("missing")));
}


ATF_TEST_CASE_WITHOUT_HEAD(from_arguments__none);
ATF_TEST_CASE_BODY(from_arguments__none)
{
    const user_files::kyuafile suite = user_files::kyuafile::from_arguments(
        cmdline::args_vector());
    ATF_REQUIRE_EQ(0, suite.test_programs().size());
}


ATF_TEST_CASE_WITHOUT_HEAD(from_arguments__some);
ATF_TEST_CASE_BODY(from_arguments__some)
{
    cmdline::args_vector args;
    args.push_back("a/b/c");
    args.push_back("foo/bar");
    const user_files::kyuafile suite = user_files::kyuafile::from_arguments(
        args);
    ATF_REQUIRE_EQ(2, suite.test_programs().size());
    ATF_REQUIRE_EQ(fs::path("a/b/c"), suite.test_programs()[0]);
    ATF_REQUIRE_EQ(fs::path("foo/bar"), suite.test_programs()[1]);
}


ATF_TEST_CASE_WITHOUT_HEAD(from_arguments__with_test_case);
ATF_TEST_CASE_BODY(from_arguments__with_test_case)
{
    cmdline::args_vector args;
    args.push_back("foo/bar:test_case");
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "not implemented",
                         user_files::kyuafile::from_arguments(args));
}


ATF_TEST_CASE_WITHOUT_HEAD(from_arguments__invalid_path);
ATF_TEST_CASE_BODY(from_arguments__invalid_path)
{
    cmdline::args_vector args;
    args.push_back("");
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "Invalid path",
                         user_files::kyuafile::from_arguments(args));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, load__current_directory);
    ATF_ADD_TEST_CASE(tcs, load__other_directory);
    ATF_ADD_TEST_CASE(tcs, load__bad_syntax);
    ATF_ADD_TEST_CASE(tcs, load__missing_file);

    ATF_ADD_TEST_CASE(tcs, from_arguments__none);
    ATF_ADD_TEST_CASE(tcs, from_arguments__some);
    ATF_ADD_TEST_CASE(tcs, from_arguments__with_test_case);
    ATF_ADD_TEST_CASE(tcs, from_arguments__invalid_path);
}
