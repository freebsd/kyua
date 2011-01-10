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

#include <atf-c++.hpp>

#include "engine/kyuafile.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/parser.hpp"

namespace cmdline = utils::cmdline;
namespace fs = utils::fs;


ATF_TEST_CASE_WITHOUT_HEAD(from_arguments__none);
ATF_TEST_CASE_BODY(from_arguments__none)
{
    const engine::kyuafile suite = engine::kyuafile::from_arguments(
        cmdline::args_vector());
    ATF_REQUIRE_EQ(0, suite.test_programs().size());
}


ATF_TEST_CASE_WITHOUT_HEAD(from_arguments__some);
ATF_TEST_CASE_BODY(from_arguments__some)
{
    cmdline::args_vector args;
    args.push_back("a/b/c");
    args.push_back("foo/bar");
    const engine::kyuafile suite = engine::kyuafile::from_arguments(
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
                         engine::kyuafile::from_arguments(args));
}


ATF_TEST_CASE_WITHOUT_HEAD(from_arguments__invalid_path);
ATF_TEST_CASE_BODY(from_arguments__invalid_path)
{
    cmdline::args_vector args;
    args.push_back("");
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "Invalid path",
                         engine::kyuafile::from_arguments(args));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, from_arguments__none);
    ATF_ADD_TEST_CASE(tcs, from_arguments__some);
    ATF_ADD_TEST_CASE(tcs, from_arguments__with_test_case);
    ATF_ADD_TEST_CASE(tcs, from_arguments__invalid_path);
}
