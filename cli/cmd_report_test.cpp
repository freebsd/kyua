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

#include "cli/cmd_report.hpp"
#include "utils/fs/path.hpp"
#include "utils/cmdline/exceptions.hpp"

namespace cmdline = utils::cmdline;
namespace fs = utils::fs;


ATF_TEST_CASE_WITHOUT_HEAD(output_option__settings);
ATF_TEST_CASE_BODY(output_option__settings)
{
    const cli::output_option o;
    ATF_REQUIRE(o.has_short_name());
    ATF_REQUIRE_EQ('o', o.short_name());
    ATF_REQUIRE_EQ("output", o.long_name());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("format:output", o.arg_name());
    ATF_REQUIRE(o.has_default_value());
    ATF_REQUIRE_EQ("console:/dev/stdout", o.default_value());
}


ATF_TEST_CASE_WITHOUT_HEAD(output_option__validate);
ATF_TEST_CASE_BODY(output_option__validate)
{
    const cli::output_option o;

    o.validate("console:/dev/stdout");
    o.validate("console:abc");

    ATF_REQUIRE_THROW_RE(cmdline::option_argument_value_error,
                         "form.*format:path", o.validate(""));
    ATF_REQUIRE_THROW_RE(cmdline::option_argument_value_error,
                         "empty", o.validate("console:"));
    ATF_REQUIRE_THROW_RE(cmdline::option_argument_value_error,
                         "Unknown output format.*foo", o.validate("foo:b"));
}


ATF_TEST_CASE_WITHOUT_HEAD(output_option__convert);
ATF_TEST_CASE_BODY(output_option__convert)
{
    using cli::output_option;

    ATF_REQUIRE(output_option::option_type(output_option::console_format,
                                           fs::path("/dev/stdout")) ==
                output_option::convert("console:/dev/stdout"));
    ATF_REQUIRE(output_option::option_type(output_option::console_format,
                                           fs::path("abcd/efg")) ==
                output_option::convert("console:abcd//efg/"));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, output_option__settings);
    ATF_ADD_TEST_CASE(tcs, output_option__validate);
    ATF_ADD_TEST_CASE(tcs, output_option__convert);
}
