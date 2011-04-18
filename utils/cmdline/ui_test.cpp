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

#include <atf-c++.hpp>

#include "utils/cmdline/globals.hpp"
#include "utils/cmdline/ui.hpp"

namespace cmdline = utils::cmdline;


namespace {


/// Trivial implementation of the ui interface for testing purposes.
class ui_test : public cmdline::ui {
public:
    std::string err_message;
    std::string out_message;

    void err(const std::string& message)
    {
        ATF_REQUIRE(err_message.empty());
        err_message = message;
    }

    void out(const std::string& message)
    {
        ATF_REQUIRE(out_message.empty());
        out_message = message;
    }
};


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(print_error);
ATF_TEST_CASE_BODY(print_error)
{
    cmdline::init("error-program");
    ui_test ui;
    cmdline::print_error(&ui, "The error");
    ATF_REQUIRE(ui.out_message.empty());
    ATF_REQUIRE_EQ("error-program: E: The error.", ui.err_message);
}


ATF_TEST_CASE_WITHOUT_HEAD(print_info);
ATF_TEST_CASE_BODY(print_info)
{
    cmdline::init("info-program");
    ui_test ui;
    cmdline::print_info(&ui, "The info");
    ATF_REQUIRE(ui.out_message.empty());
    ATF_REQUIRE_EQ("info-program: I: The info.", ui.err_message);
}


ATF_TEST_CASE_WITHOUT_HEAD(print_warning);
ATF_TEST_CASE_BODY(print_warning)
{
    cmdline::init("warning-program");
    ui_test ui;
    cmdline::print_warning(&ui, "The warning");
    ATF_REQUIRE(ui.out_message.empty());
    ATF_REQUIRE_EQ("warning-program: W: The warning.", ui.err_message);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, print_error);
    ATF_ADD_TEST_CASE(tcs, print_info);
    ATF_ADD_TEST_CASE(tcs, print_warning);
}
