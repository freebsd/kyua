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

#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/options.hpp"

namespace cmdline = utils::cmdline;

namespace {


class mock_option : public cmdline::base_option {
public:
    mock_option(const char short_name_, const char* long_name_,
                  const char* description_, const char* arg_name_ = NULL,
                  const char* default_value_ = NULL) :
        base_option(short_name_, long_name_, description_, arg_name_,
                    default_value_) {}
    mock_option(const char* long_name_,
                  const char* description_, const char* arg_name_ = NULL,
                  const char* default_value_ = NULL) :
        base_option(long_name_, description_, arg_name_, default_value_) {}
    virtual ~mock_option(void) {}

    typedef std::string option_type;

    virtual void
    validate(const std::string& str) const
    {
        // Do nothing.
    }

    static std::string
    convert(const std::string& str)
    {
        return str;
    }
};


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(base_option__short_name__no_arg);
ATF_TEST_CASE_BODY(base_option__short_name__no_arg)
{
    const mock_option o('f', "force", "Force execution");
    ATF_REQUIRE(o.has_short_name());
    ATF_REQUIRE_EQ('f', o.short_name());
    ATF_REQUIRE_EQ("force", o.long_name());
    ATF_REQUIRE_EQ("Force execution", o.description());
    ATF_REQUIRE(!o.needs_arg());
    ATF_REQUIRE_EQ("-f", o.format_short_name());
    ATF_REQUIRE_EQ("--force", o.format_long_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(base_option__short_name__with_arg__no_default);
ATF_TEST_CASE_BODY(base_option__short_name__with_arg__no_default)
{
    const mock_option o('c', "conf_file", "Configuration file", "path");
    ATF_REQUIRE(o.has_short_name());
    ATF_REQUIRE_EQ('c', o.short_name());
    ATF_REQUIRE_EQ("conf_file", o.long_name());
    ATF_REQUIRE_EQ("Configuration file", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("path", o.arg_name());
    ATF_REQUIRE(!o.has_default_value());
    ATF_REQUIRE_EQ("-c path", o.format_short_name());
    ATF_REQUIRE_EQ("--conf_file=path", o.format_long_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(base_option__short_name__with_arg__with_default);
ATF_TEST_CASE_BODY(base_option__short_name__with_arg__with_default)
{
    const mock_option o('c', "conf_file", "Configuration file", "path",
                        "defpath");
    ATF_REQUIRE(o.has_short_name());
    ATF_REQUIRE_EQ('c', o.short_name());
    ATF_REQUIRE_EQ("conf_file", o.long_name());
    ATF_REQUIRE_EQ("Configuration file", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("path", o.arg_name());
    ATF_REQUIRE(o.has_default_value());
    ATF_REQUIRE_EQ("defpath", o.default_value());
    ATF_REQUIRE_EQ("-c path", o.format_short_name());
    ATF_REQUIRE_EQ("--conf_file=path", o.format_long_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(base_option__long_name__no_arg);
ATF_TEST_CASE_BODY(base_option__long_name__no_arg)
{
    const mock_option o("dryrun", "Dry run mode");
    ATF_REQUIRE(!o.has_short_name());
    ATF_REQUIRE_EQ("dryrun", o.long_name());
    ATF_REQUIRE_EQ("Dry run mode", o.description());
    ATF_REQUIRE(!o.needs_arg());
    ATF_REQUIRE_EQ("--dryrun", o.format_long_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(base_option__long_name__with_arg__no_default);
ATF_TEST_CASE_BODY(base_option__long_name__with_arg__no_default)
{
    const mock_option o("helper", "Path to helper", "path");
    ATF_REQUIRE(!o.has_short_name());
    ATF_REQUIRE_EQ("helper", o.long_name());
    ATF_REQUIRE_EQ("Path to helper", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("path", o.arg_name());
    ATF_REQUIRE(!o.has_default_value());
    ATF_REQUIRE_EQ("--helper=path", o.format_long_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(base_option__long_name__with_arg__with_default);
ATF_TEST_CASE_BODY(base_option__long_name__with_arg__with_default)
{
    const mock_option o("executable", "Executable name", "file", "foo");
    ATF_REQUIRE(!o.has_short_name());
    ATF_REQUIRE_EQ("executable", o.long_name());
    ATF_REQUIRE_EQ("Executable name", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("file", o.arg_name());
    ATF_REQUIRE(o.has_default_value());
    ATF_REQUIRE_EQ("foo", o.default_value());
    ATF_REQUIRE_EQ("--executable=file", o.format_long_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(bool_option__short_name);
ATF_TEST_CASE_BODY(bool_option__short_name)
{
    const cmdline::bool_option o('f', "force", "Force execution");
    ATF_REQUIRE(o.has_short_name());
    ATF_REQUIRE_EQ('f', o.short_name());
    ATF_REQUIRE_EQ("force", o.long_name());
    ATF_REQUIRE_EQ("Force execution", o.description());
    ATF_REQUIRE(!o.needs_arg());
}


ATF_TEST_CASE_WITHOUT_HEAD(bool_option__long_name);
ATF_TEST_CASE_BODY(bool_option__long_name)
{
    const cmdline::bool_option o("force", "Force execution");
    ATF_REQUIRE(!o.has_short_name());
    ATF_REQUIRE_EQ("force", o.long_name());
    ATF_REQUIRE_EQ("Force execution", o.description());
    ATF_REQUIRE(!o.needs_arg());
}


ATF_TEST_CASE_WITHOUT_HEAD(path_option__short_name);
ATF_TEST_CASE_BODY(path_option__short_name)
{
    const cmdline::path_option o('p', "path", "The path", "arg", "value");
    ATF_REQUIRE(o.has_short_name());
    ATF_REQUIRE_EQ('p', o.short_name());
    ATF_REQUIRE_EQ("path", o.long_name());
    ATF_REQUIRE_EQ("The path", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("arg", o.arg_name());
    ATF_REQUIRE(o.has_default_value());
    ATF_REQUIRE_EQ("value", o.default_value());
}


ATF_TEST_CASE_WITHOUT_HEAD(path_option__long_name);
ATF_TEST_CASE_BODY(path_option__long_name)
{
    const cmdline::path_option o("path", "The path", "arg", "value");
    ATF_REQUIRE(!o.has_short_name());
    ATF_REQUIRE_EQ("path", o.long_name());
    ATF_REQUIRE_EQ("The path", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("arg", o.arg_name());
    ATF_REQUIRE(o.has_default_value());
    ATF_REQUIRE_EQ("value", o.default_value());
}


ATF_TEST_CASE_WITHOUT_HEAD(path_option__type);
ATF_TEST_CASE_BODY(path_option__type)
{
    const cmdline::path_option o("path", "The path", "arg");

    o.validate("/some/path");

    try {
        o.validate("");
        fail("option_argument_value_error not raised");
    } catch (const cmdline::option_argument_value_error& e) {
        // Expected; ignore.
    }

    const cmdline::path_option::option_type path =
        cmdline::path_option::convert("/foo/bar");
    ATF_REQUIRE_EQ("bar", path.leaf_name());  // Ensure valid type.
}


ATF_TEST_CASE_WITHOUT_HEAD(string_option__short_name);
ATF_TEST_CASE_BODY(string_option__short_name)
{
    const cmdline::string_option o('p', "string", "The string", "arg", "value");
    ATF_REQUIRE(o.has_short_name());
    ATF_REQUIRE_EQ('p', o.short_name());
    ATF_REQUIRE_EQ("string", o.long_name());
    ATF_REQUIRE_EQ("The string", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("arg", o.arg_name());
    ATF_REQUIRE(o.has_default_value());
    ATF_REQUIRE_EQ("value", o.default_value());
}


ATF_TEST_CASE_WITHOUT_HEAD(string_option__long_name);
ATF_TEST_CASE_BODY(string_option__long_name)
{
    const cmdline::string_option o("string", "The string", "arg", "value");
    ATF_REQUIRE(!o.has_short_name());
    ATF_REQUIRE_EQ("string", o.long_name());
    ATF_REQUIRE_EQ("The string", o.description());
    ATF_REQUIRE(o.needs_arg());
    ATF_REQUIRE_EQ("arg", o.arg_name());
    ATF_REQUIRE(o.has_default_value());
    ATF_REQUIRE_EQ("value", o.default_value());
}


ATF_TEST_CASE_WITHOUT_HEAD(string_option__type);
ATF_TEST_CASE_BODY(string_option__type)
{
    const cmdline::string_option o("string", "The string", "foo");

    o.validate("");
    o.validate("some string");

    const cmdline::string_option::option_type string =
        cmdline::string_option::convert("foo");
    ATF_REQUIRE_EQ(3, string.length());  // Ensure valid type.
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, base_option__short_name__no_arg);
    ATF_ADD_TEST_CASE(tcs, base_option__short_name__with_arg__no_default);
    ATF_ADD_TEST_CASE(tcs, base_option__short_name__with_arg__with_default);
    ATF_ADD_TEST_CASE(tcs, base_option__long_name__no_arg);
    ATF_ADD_TEST_CASE(tcs, base_option__long_name__with_arg__no_default);
    ATF_ADD_TEST_CASE(tcs, base_option__long_name__with_arg__with_default);
    ATF_ADD_TEST_CASE(tcs, bool_option__short_name);
    ATF_ADD_TEST_CASE(tcs, bool_option__long_name);
    ATF_ADD_TEST_CASE(tcs, path_option__short_name);
    ATF_ADD_TEST_CASE(tcs, path_option__long_name);
    ATF_ADD_TEST_CASE(tcs, path_option__type);
    ATF_ADD_TEST_CASE(tcs, string_option__short_name);
    ATF_ADD_TEST_CASE(tcs, string_option__long_name);
    ATF_ADD_TEST_CASE(tcs, string_option__type);
}
