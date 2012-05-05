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

#include "utils/config/tree.ipp"

#include <atf-c++.hpp>

namespace config = utils::config;


ATF_TEST_CASE_WITHOUT_HEAD(define_set_lookup__one_level);
ATF_TEST_CASE_BODY(define_set_lookup__one_level)
{
    config::tree tree;

    tree.define< config::int_node >("var1");
    tree.define< config::string_node >("var2");
    tree.define< config::int_node >("var3");

    tree.set< config::int_node >("var1", 42);
    tree.set< config::string_node >("var2", "hello");
    tree.set< config::int_node >("var3", 12345);

    ATF_REQUIRE_EQ(42, tree.lookup< config::int_node >("var1"));
    ATF_REQUIRE_EQ("hello", tree.lookup< config::string_node >("var2"));
    ATF_REQUIRE_EQ(12345, tree.lookup< config::int_node >("var3"));
}


ATF_TEST_CASE_WITHOUT_HEAD(define_set_lookup__multiple_levels);
ATF_TEST_CASE_BODY(define_set_lookup__multiple_levels)
{
    config::tree tree;

    tree.define< config::int_node >("foo.bar.1");
    tree.define< config::string_node >("foo.bar.2");
    tree.define< config::int_node >("foo.3");
    tree.define_dynamic("sub.tree");

    tree.set< config::int_node >("foo.bar.1", 42);
    tree.set< config::string_node >("foo.bar.2", "hello");
    tree.set< config::int_node >("foo.3", 12345);
    tree.set< config::string_node >("sub.tree.1", "bye");
    tree.set< config::int_node >("sub.tree.2", 4);
    tree.set< config::int_node >("sub.tree.3.4", 123);

    ATF_REQUIRE_EQ(42, tree.lookup< config::int_node >("foo.bar.1"));
    ATF_REQUIRE_EQ("hello", tree.lookup< config::string_node >("foo.bar.2"));
    ATF_REQUIRE_EQ(12345, tree.lookup< config::int_node >("foo.3"));
    ATF_REQUIRE_EQ(4, tree.lookup< config::int_node >("sub.tree.2"));
    ATF_REQUIRE_EQ(123, tree.lookup< config::int_node >("sub.tree.3.4"));
}


ATF_TEST_CASE_WITHOUT_HEAD(lookup__invalid_key);
ATF_TEST_CASE_BODY(lookup__invalid_key)
{
    config::tree tree;

    ATF_REQUIRE_THROW(config::invalid_key_error,
                      tree.lookup< config::int_node >(""));
    ATF_REQUIRE_THROW(config::invalid_key_error,
                      tree.lookup< config::int_node >("."));
    ATF_REQUIRE_THROW(config::invalid_key_error,
                      tree.lookup< config::int_node >("foo."));
    ATF_REQUIRE_THROW(config::invalid_key_error,
                      tree.lookup< config::int_node >(".foo"));
    ATF_REQUIRE_THROW(config::invalid_key_error,
                      tree.lookup< config::int_node >("foo..bar"));
}


ATF_TEST_CASE_WITHOUT_HEAD(lookup__unknown_key);
ATF_TEST_CASE_BODY(lookup__unknown_key)
{
    config::tree tree;

    tree.define< config::int_node >("foo.bar");
    tree.define< config::int_node >("a.b.c");
    tree.define_dynamic("a.d");
    tree.set< config::int_node >("a.b.c", 123);
    tree.set< config::int_node >("a.d.100", 0);

    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("abc"));

    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("foo"));
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("foo.bar"));
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("foo.bar.baz"));

    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("a"));
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("a.b"));
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("a.c"));
    (void)tree.lookup< config::int_node >("a.b.c");
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("a.b.c.d"));
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("a.d"));
    (void)tree.lookup< config::int_node >("a.d.100");
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("a.d.101"));
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("a.d.100.3"));
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("a.d.e"));
}


ATF_TEST_CASE_WITHOUT_HEAD(set__invalid_key);
ATF_TEST_CASE_BODY(set__invalid_key)
{
    config::tree tree;

    ATF_REQUIRE_THROW(config::invalid_key_error,
                      tree.set< config::int_node >("", 3));
    ATF_REQUIRE_THROW(config::invalid_key_error,
                      tree.set< config::int_node >(".", 18));
    ATF_REQUIRE_THROW(config::invalid_key_error,
                      tree.set< config::int_node >("foo.", 54));
    ATF_REQUIRE_THROW(config::invalid_key_error,
                      tree.set< config::int_node >(".foo", 28));
    ATF_REQUIRE_THROW(config::invalid_key_error,
                      tree.set< config::int_node >("foo..bar", 43));
}


ATF_TEST_CASE_WITHOUT_HEAD(set__unknown_key);
ATF_TEST_CASE_BODY(set__unknown_key)
{
    config::tree tree;

    tree.define< config::int_node >("foo.bar");
    tree.define< config::int_node >("a.b.c");
    tree.define_dynamic("a.d");
    tree.set< config::int_node >("a.b.c", 123);
    tree.set< config::string_node >("a.d.3", "foo");

    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.set< config::int_node >("abc", 2));

    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.set< config::int_node >("foo", 3));
    tree.set< config::int_node >("foo.bar", 15);
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.set< config::int_node >("foo.bar.baz", 0));

    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.set< config::int_node >("a", -10));
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.set< config::int_node >("a.b", 20));
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.set< config::int_node >("a.c", 100));
    tree.set< config::int_node >("a.b.c", -3);
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.set< config::int_node >("a.b.c.d", 82));
    tree.set< config::string_node >("a.d.3", "bar");
    tree.set< config::string_node >("a.d.4", "bar");
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.set< config::int_node >("a.d.4.5", 82));
    tree.set< config::int_node >("a.d.5.6", 82);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, define_set_lookup__one_level);
    ATF_ADD_TEST_CASE(tcs, define_set_lookup__multiple_levels);

    ATF_ADD_TEST_CASE(tcs, lookup__invalid_key);
    ATF_ADD_TEST_CASE(tcs, lookup__unknown_key);

    ATF_ADD_TEST_CASE(tcs, set__invalid_key);
    ATF_ADD_TEST_CASE(tcs, set__unknown_key);
}
