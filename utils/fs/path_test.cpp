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

#include <set>

#include <atf-c++.hpp>

#include "utils/fs/exceptions.hpp"
#include "utils/fs/path.hpp"

using utils::fs::invalid_path_error;
using utils::fs::path;


ATF_TEST_CASE_WITHOUT_HEAD(normalize__ok);
ATF_TEST_CASE_BODY(normalize__ok)
{
    ATF_REQUIRE_EQ(".", path(".").str());
    ATF_REQUIRE_EQ("..", path("..").str());
    ATF_REQUIRE_EQ("/", path("/").str());
    ATF_REQUIRE_EQ("/", path("///").str());

    ATF_REQUIRE_EQ("foo", path("foo").str());
    ATF_REQUIRE_EQ("foo/bar", path("foo/bar").str());
    ATF_REQUIRE_EQ("foo/bar", path("foo/bar/").str());

    ATF_REQUIRE_EQ("/foo", path("/foo").str());
    ATF_REQUIRE_EQ("/foo/bar", path("/foo/bar").str());
    ATF_REQUIRE_EQ("/foo/bar", path("/foo/bar/").str());

    ATF_REQUIRE_EQ("/foo", path("///foo").str());
    ATF_REQUIRE_EQ("/foo/bar", path("///foo///bar").str());
    ATF_REQUIRE_EQ("/foo/bar", path("///foo///bar///").str());
}


ATF_TEST_CASE_WITHOUT_HEAD(normalize__invalid);
ATF_TEST_CASE_BODY(normalize__invalid)
{
    try {
        path("");
        fail("invalid_path_error not raised");
    } catch (const invalid_path_error& e) {
        ATF_REQUIRE(e.invalid_path().empty());
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(is_absolute);
ATF_TEST_CASE_BODY(is_absolute)
{
    ATF_REQUIRE( path("/").is_absolute());
    ATF_REQUIRE( path("////").is_absolute());
    ATF_REQUIRE( path("////a").is_absolute());
    ATF_REQUIRE( path("//a//").is_absolute());
    ATF_REQUIRE(!path("a////").is_absolute());
    ATF_REQUIRE(!path("../foo").is_absolute());
}


ATF_TEST_CASE_WITHOUT_HEAD(branch_path);
ATF_TEST_CASE_BODY(branch_path)
{
    ATF_REQUIRE_EQ(".", path(".").branch_path().str());
    ATF_REQUIRE_EQ(".", path("foo").branch_path().str());
    ATF_REQUIRE_EQ("foo", path("foo/bar").branch_path().str());
    ATF_REQUIRE_EQ("/", path("/foo").branch_path().str());
    ATF_REQUIRE_EQ("/foo", path("/foo/bar").branch_path().str());
}


ATF_TEST_CASE_WITHOUT_HEAD(leaf_name);
ATF_TEST_CASE_BODY(leaf_name)
{
    ATF_REQUIRE_EQ(".", path(".").leaf_name());
    ATF_REQUIRE_EQ("foo", path("foo").leaf_name());
    ATF_REQUIRE_EQ("bar", path("foo/bar").leaf_name());
    ATF_REQUIRE_EQ("foo", path("/foo").leaf_name());
    ATF_REQUIRE_EQ("bar", path("/foo/bar").leaf_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(compare_less_than);
ATF_TEST_CASE_BODY(compare_less_than)
{
    ATF_REQUIRE(!(path("/") < path("/")));
    ATF_REQUIRE(!(path("/") < path("///")));

    ATF_REQUIRE(!(path("/a/b/c") < path("/a/b/c")));

    ATF_REQUIRE(  path("/a") < path("/b"));
    ATF_REQUIRE(!(path("/b") < path("/a")));

    ATF_REQUIRE(  path("/a") < path("/aa"));
    ATF_REQUIRE(!(path("/aa") < path("/a")));
}


ATF_TEST_CASE_WITHOUT_HEAD(compare_equal);
ATF_TEST_CASE_BODY(compare_equal)
{
    ATF_REQUIRE(path("/") == path("///"));
    ATF_REQUIRE(path("/a") == path("///a"));
    ATF_REQUIRE(path("/a") == path("///a///"));

    ATF_REQUIRE(path("a/b/c") == path("a//b//c"));
    ATF_REQUIRE(path("a/b/c") == path("a//b//c///"));
}


ATF_TEST_CASE_WITHOUT_HEAD(compare_different);
ATF_TEST_CASE_BODY(compare_different)
{
    ATF_REQUIRE(path("/") != path("//a/"));
    ATF_REQUIRE(path("/a") != path("a///"));

    ATF_REQUIRE(path("a/b/c") != path("a/b"));
    ATF_REQUIRE(path("a/b/c") != path("a//b"));
    ATF_REQUIRE(path("a/b/c") != path("/a/b/c"));
    ATF_REQUIRE(path("a/b/c") != path("/a//b//c"));
}


ATF_TEST_CASE_WITHOUT_HEAD(concat);
ATF_TEST_CASE_BODY(concat)
{
    ATF_REQUIRE_EQ("foo/bar", (path("foo") / "bar").str());
    ATF_REQUIRE_EQ("foo/bar", (path("foo/") / "bar").str());
    ATF_REQUIRE_EQ("foo/bar/baz", (path("foo/") / "bar//baz///").str());
}


ATF_TEST_CASE_WITHOUT_HEAD(use_as_key);
ATF_TEST_CASE_BODY(use_as_key)
{
    std::set< path > paths;
    paths.insert(path("/a"));
    ATF_REQUIRE(paths.find(path("//a")) != paths.end());
    ATF_REQUIRE(paths.find(path("a")) == paths.end());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, normalize__ok);
    ATF_ADD_TEST_CASE(tcs, normalize__invalid);
    ATF_ADD_TEST_CASE(tcs, is_absolute);
    ATF_ADD_TEST_CASE(tcs, branch_path);
    ATF_ADD_TEST_CASE(tcs, leaf_name);
    ATF_ADD_TEST_CASE(tcs, compare_less_than);
    ATF_ADD_TEST_CASE(tcs, compare_equal);
    ATF_ADD_TEST_CASE(tcs, compare_different);
    ATF_ADD_TEST_CASE(tcs, concat);
    ATF_ADD_TEST_CASE(tcs, use_as_key);
}
