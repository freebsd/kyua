// Copyright 2012 Google Inc.
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

#include "utils/config/parser.hpp"

#include <fstream>
#include <stdexcept>

#include <atf-c++.hpp>

#include "utils/config/exceptions.hpp"
#include "utils/config/parser.hpp"
#include "utils/config/tree.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"

namespace config = utils::config;
namespace fs = utils::fs;


namespace {


/// Implementation of a parser for testing purposes.
class mock_parser : public config::parser {
    /// Initializes the tree keys before reading the file.
    ///
    /// \param [in,out] tree The tree in which to define the key structure.
    /// \param syntax_format The name of the file format as specified in the
    ///     configuration file.
    /// \param syntax_version The version of the file format as specified in the
    ///     configuration file.
    void
    setup(config::tree& tree,
          const std::string& syntax_format,
          const int syntax_version)
    {
        if (syntax_format == "no-keys" && syntax_version == 1) {
            // Do nothing on config_tree.
        } else if (syntax_format == "some-keys" && syntax_version == 2) {
            tree.define< config::string_node >("top_string");
            tree.define< config::int_node >("inner.int");
            tree.define_dynamic("inner.dynamic");
        } else {
            throw std::runtime_error(F("Unknown syntax %s, version %s") %
                                     syntax_format % syntax_version);
        }
    }

public:
    /// Initializes a parser.
    mock_parser(config::tree& tree) :
        config::parser(tree)
    {
    }
};


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(no_keys__ok);
ATF_TEST_CASE_BODY(no_keys__ok)
{
    std::ofstream output("output.lua");
    output << "syntax('no-keys', 1)\n";
    output << "local foo = 'value'\n";
    output.flush();

    config::tree tree;
    mock_parser(tree).parse(fs::path("output.lua"));
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::string_node >("foo"));
}


ATF_TEST_CASE_WITHOUT_HEAD(no_keys__unknown_key);
ATF_TEST_CASE_BODY(no_keys__unknown_key)
{
    std::ofstream output("output.lua");
    output << "syntax('no-keys', 1)\n";
    output << "foo = 'value'\n";
    output.flush();

    config::tree tree;
    ATF_REQUIRE_THROW_RE(config::syntax_error, "foo",
                         mock_parser(tree).parse(fs::path("output.lua")));
}


ATF_TEST_CASE_WITHOUT_HEAD(some_keys__ok);
ATF_TEST_CASE_BODY(some_keys__ok)
{
    std::ofstream output("output.lua");
    output << "syntax('some-keys', 2)\n";
    output << "top_string = 'foo'\n";
    output << "inner.int = 12345\n";
    output << "inner.dynamic.foo = 78\n";
    output << "inner.dynamic.bar = false\n";
    output.flush();

    config::tree tree;
    mock_parser(tree).parse(fs::path("output.lua"));
    ATF_REQUIRE_EQ("foo", tree.lookup< config::string_node >("top_string"));
    ATF_REQUIRE_EQ(12345, tree.lookup< config::int_node >("inner.int"));
    ATF_REQUIRE_EQ(78, tree.lookup< config::int_node >("inner.dynamic.foo"));
    ATF_REQUIRE(!tree.lookup< config::bool_node >("inner.dynamic.bar"));
}


ATF_TEST_CASE_WITHOUT_HEAD(some_keys__unknown_key);
ATF_TEST_CASE_BODY(some_keys__unknown_key)
{
    {
        std::ofstream output("output.lua");
        output << "syntax('some-keys', 2)\n";
        output << "top_string2 = 'foo'\n";
    }
    config::tree tree1;
    ATF_REQUIRE_THROW_RE(config::syntax_error,
                         "Unknown key 'top_string2'",
                         mock_parser(tree1).parse(fs::path("output.lua")));

    {
        std::ofstream output("output.lua");
        output << "syntax('some-keys', 2)\n";
        output << "inner.int2 = 12345\n";
    }
    config::tree tree2;
    ATF_REQUIRE_THROW_RE(config::syntax_error,
                         "Unknown key 'inner.int2'",
                         mock_parser(tree2).parse(fs::path("output.lua")));
}


ATF_TEST_CASE_WITHOUT_HEAD(invalid_syntax);
ATF_TEST_CASE_BODY(invalid_syntax)
{
    config::tree tree;

    {
        std::ofstream output("output.lua");
        output << "syntax('invalid', 1234)\n";
    }
    ATF_REQUIRE_THROW_RE(config::syntax_error,
                         "Unknown syntax invalid, version 1234",
                         mock_parser(tree).parse(fs::path("output.lua")));

    {
        std::ofstream output("output.lua");
        output << "syntax('no-keys', 2)\n";
    }
    ATF_REQUIRE_THROW_RE(config::syntax_error,
                         "Unknown syntax no-keys, version 2",
                         mock_parser(tree).parse(fs::path("output.lua")));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, no_keys__ok);
    ATF_ADD_TEST_CASE(tcs, no_keys__unknown_key);

    ATF_ADD_TEST_CASE(tcs, some_keys__ok);
    ATF_ADD_TEST_CASE(tcs, some_keys__unknown_key);

    ATF_ADD_TEST_CASE(tcs, invalid_syntax);
}
