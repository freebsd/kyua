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

#include <cstdlib>

#include <atf-c++.hpp>

#include "cli/cmd_config.hpp"
#include "engine/user_files/config.hpp"
#include "utils/cmdline/globals.hpp"
#include "utils/cmdline/ui_mock.hpp"
#include "utils/optional.ipp"
#include "utils/test_utils.hpp"

namespace cmdline = utils::cmdline;
namespace user_files = engine::user_files;

using cli::cmd_config;
using utils::none;


namespace {


static user_files::config
fake_config(void)
{
    user_files::test_suites_map test_suites;
    {
        user_files::properties_map props;
        props["bar"] = "first";
        props["baz"] = "second";
        test_suites["foo"] = props;
    }

    return user_files::config("the-architecture", "the-platform", none,
                              test_suites);
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(all);
ATF_TEST_CASE_BODY(all)
{
    cmdline::args_vector args;
    args.push_back("config");

    cmd_config cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args, fake_config()));

    ATF_REQUIRE_EQ(4, ui.out_log().size());
    ATF_REQUIRE_EQ("architecture = the-architecture", ui.out_log()[0]);
    ATF_REQUIRE_EQ("foo.bar = first", ui.out_log()[1]);
    ATF_REQUIRE_EQ("foo.baz = second", ui.out_log()[2]);
    ATF_REQUIRE_EQ("platform = the-platform", ui.out_log()[3]);
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(some__ok);
ATF_TEST_CASE_BODY(some__ok)
{
    cmdline::args_vector args;
    args.push_back("config");
    args.push_back("platform");
    args.push_back("foo.baz");

    cmd_config cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args, fake_config()));

    ATF_REQUIRE_EQ(2, ui.out_log().size());
    ATF_REQUIRE_EQ("platform = the-platform", ui.out_log()[0]);
    ATF_REQUIRE_EQ("foo.baz = second", ui.out_log()[1]);
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(some__fail);
ATF_TEST_CASE_BODY(some__fail)
{
    cmdline::args_vector args;
    args.push_back("config");
    args.push_back("platform");
    args.push_back("unknown");
    args.push_back("foo.baz");

    cmdline::init("progname");

    cmd_config cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_FAILURE, cmd.main(&ui, args, fake_config()));

    ATF_REQUIRE_EQ(2, ui.out_log().size());
    ATF_REQUIRE_EQ("platform = the-platform", ui.out_log()[0]);
    ATF_REQUIRE_EQ("foo.baz = second", ui.out_log()[1]);
    ATF_REQUIRE_EQ(1, ui.err_log().size());
    ATF_REQUIRE(utils::grep_string("unknown.*not defined", ui.err_log()[0]));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, all);
    ATF_ADD_TEST_CASE(tcs, some__ok);
    ATF_ADD_TEST_CASE(tcs, some__fail);
}
