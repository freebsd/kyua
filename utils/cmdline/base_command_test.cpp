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

#include "utils/cmdline/base_command.ipp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui_mock.hpp"

namespace cmdline = utils::cmdline;


namespace {


class mock_cmd : public utils::cmdline::base_command {
public:
    bool executed;
    std::string optvalue;

    mock_cmd(void) :
        cmdline::base_command("mock", "arg1 [arg2 [arg3]]", 1, 3,
                              "Command for testing."),
        executed(false)
    {
        add_option(cmdline::string_option("the_string", "Test option", "arg"));
    }

    int
    run(utils::cmdline::ui* ui, const utils::cmdline::parsed_cmdline& cmdline)
    {
        if (cmdline.has_option("the_string"))
            optvalue = cmdline.get_option< cmdline::string_option >(
                "the_string");
        executed = true;
        return 1234;
    }
};


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(getters);
ATF_TEST_CASE_BODY(getters)
{
    mock_cmd cmd;
    ATF_REQUIRE_EQ("mock", cmd.name());
    ATF_REQUIRE_EQ("arg1 [arg2 [arg3]]", cmd.arg_list());
    ATF_REQUIRE_EQ("Command for testing.", cmd.short_description());
    ATF_REQUIRE_EQ(1, cmd.options().size());
    ATF_REQUIRE_EQ("the_string", cmd.options()[0]->long_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(main__parse_ok);
ATF_TEST_CASE_BODY(main__parse_ok)
{
    mock_cmd cmd;

    cmdline::ui_mock ui;
    cmdline::args_vector args;
    args.push_back("mock");
    args.push_back("--the_string=foo bar");
    args.push_back("one arg");
    args.push_back("another arg");
    ATF_REQUIRE_EQ(1234, cmd.main(&ui, args));
    ATF_REQUIRE(cmd.executed);
    ATF_REQUIRE_EQ("foo bar", cmd.optvalue);
}


ATF_TEST_CASE_WITHOUT_HEAD(main__parse_fail);
ATF_TEST_CASE_BODY(main__parse_fail)
{
    mock_cmd cmd;

    cmdline::ui_mock ui;
    cmdline::args_vector args;
    args.push_back("mock");
    args.push_back("--foo-bar");
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "Unknown.*foo-bar",
                         cmd.main(&ui, args));
    ATF_REQUIRE(!cmd.executed);
}


ATF_TEST_CASE_WITHOUT_HEAD(main__valid_args);
ATF_TEST_CASE_BODY(main__valid_args)
{
    mock_cmd cmd;

    cmdline::ui_mock ui;
    cmdline::args_vector args;
    args.push_back("mock");
    args.push_back("one arg");
    ATF_REQUIRE_EQ(1234, cmd.main(&ui, args));
    ATF_REQUIRE(cmd.executed);
    ATF_REQUIRE(cmd.optvalue.empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(main__invalid_args);
ATF_TEST_CASE_BODY(main__invalid_args)
{
    mock_cmd cmd;

    cmdline::ui_mock ui;
    cmdline::args_vector args;
    args.push_back("mock");

    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "Not enough arguments",
                         cmd.main(&ui, args));
    ATF_REQUIRE(!cmd.executed);

    args.push_back("1");
    args.push_back("2");
    args.push_back("3");
    args.push_back("4");
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "Too many arguments",
                         cmd.main(&ui, args));
    ATF_REQUIRE(!cmd.executed);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, getters);
    ATF_ADD_TEST_CASE(tcs, main__parse_ok);
    ATF_ADD_TEST_CASE(tcs, main__parse_fail);
    ATF_ADD_TEST_CASE(tcs, main__valid_args);
    ATF_ADD_TEST_CASE(tcs, main__invalid_args);
}
