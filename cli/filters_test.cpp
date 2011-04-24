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

#include <stdexcept>

#include <atf-c++.hpp>

#include "cli/filters.hpp"
#include "engine/test_case.hpp"

namespace fs = utils::fs;


namespace {


/// Syntactic sugar to instantiate cli::test_filter objects.
inline cli::test_filter
mkfilter(const char* test_program, const char* test_case)
{
    return cli::test_filter(fs::path(test_program), test_case);
}


/// Syntactic sugar to instantiate engine::test_case_id objects.
inline engine::test_case_id
mkid(const char* test_program, const char* test_case)
{
    return engine::test_case_id(fs::path(test_program), test_case);
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__public_fields);
ATF_TEST_CASE_BODY(test_filter__public_fields)
{
    const cli::test_filter::test_filter filter(fs::path("foo/bar"), "baz");
    ATF_REQUIRE_EQ(fs::path("foo/bar"), filter.test_program);
    ATF_REQUIRE_EQ("baz", filter.test_case);
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__parse__ok);
ATF_TEST_CASE_BODY(test_filter__parse__ok)
{
    const cli::test_filter::test_filter filter(cli::test_filter::parse("foo"));
    ATF_REQUIRE_EQ(fs::path("foo"), filter.test_program);
    ATF_REQUIRE(filter.test_case.empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__parse__empty);
ATF_TEST_CASE_BODY(test_filter__parse__empty)
{
    ATF_REQUIRE_THROW_RE(std::runtime_error, "empty",
                         cli::test_filter::parse(""));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__parse__absolute);
ATF_TEST_CASE_BODY(test_filter__parse__absolute)
{
    ATF_REQUIRE_THROW_RE(std::runtime_error, "'/foo/bar'.*relative",
                         cli::test_filter::parse("/foo//bar"));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__parse__bad_program_name);
ATF_TEST_CASE_BODY(test_filter__parse__bad_program_name)
{
    ATF_REQUIRE_THROW_RE(std::runtime_error, "Program name.*':foo'",
                         cli::test_filter::parse(":foo"));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__parse__bad_test_case);
ATF_TEST_CASE_BODY(test_filter__parse__bad_test_case)
{
    ATF_REQUIRE_THROW_RE(std::runtime_error, "Test case.*'bar/baz:'",
                         cli::test_filter::parse("bar/baz:"));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__parse__bad_path);
ATF_TEST_CASE_BODY(test_filter__parse__bad_path)
{
    // TODO(jmmv): Not implemented.  At the moment, the only reason for a path
    // to be invalid is if it is empty... but we are checking this exact
    // condition ourselves as part of the input validation.  So we can't mock in
    // an argument with an invalid non-empty path...
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__str);
ATF_TEST_CASE_BODY(test_filter__str)
{
    const cli::test_filter filter(fs::path("foo/bar"), "baz");
    ATF_REQUIRE_EQ("foo/bar:baz", filter.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__contains__same);
ATF_TEST_CASE_BODY(test_filter__contains__same)
{
    {
        const cli::test_filter f(fs::path("foo/bar"), "baz");
        ATF_REQUIRE(f.contains(f));
    }
    {
        const cli::test_filter f(fs::path("foo/bar"), "");
        ATF_REQUIRE(f.contains(f));
    }
    {
        const cli::test_filter f(fs::path("foo"), "");
        ATF_REQUIRE(f.contains(f));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__contains__different);
ATF_TEST_CASE_BODY(test_filter__contains__different)
{
    {
        const cli::test_filter f1(fs::path("foo"), "");
        const cli::test_filter f2(fs::path("foo"), "bar");
        ATF_REQUIRE( f1.contains(f2));
        ATF_REQUIRE(!f2.contains(f1));
    }
    {
        const cli::test_filter f1(fs::path("foo/bar"), "");
        const cli::test_filter f2(fs::path("foo/bar"), "baz");
        ATF_REQUIRE( f1.contains(f2));
        ATF_REQUIRE(!f2.contains(f1));
    }
    {
        const cli::test_filter f1(fs::path("foo/bar"), "");
        const cli::test_filter f2(fs::path("foo/baz"), "");
        ATF_REQUIRE(!f1.contains(f2));
        ATF_REQUIRE(!f2.contains(f1));
    }
    {
        const cli::test_filter f1(fs::path("foo"), "");
        const cli::test_filter f2(fs::path("foo/bar"), "");
        ATF_REQUIRE( f1.contains(f2));
        ATF_REQUIRE(!f2.contains(f1));
    }
    {
        const cli::test_filter f1(fs::path("foo"), "bar");
        const cli::test_filter f2(fs::path("foo/bar"), "");
        ATF_REQUIRE(!f1.contains(f2));
        ATF_REQUIRE(!f2.contains(f1));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__matches_test_program)
ATF_TEST_CASE_BODY(test_filter__matches_test_program)
{
    {
        const cli::test_filter f(fs::path("top"), "unused");
        ATF_REQUIRE( f.matches_test_program(fs::path("top")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("top2")));
    }

    {
        const cli::test_filter f(fs::path("dir1/dir2"), "");
        ATF_REQUIRE( f.matches_test_program(fs::path("dir1/dir2/foo")));
        ATF_REQUIRE( f.matches_test_program(fs::path("dir1/dir2/bar")));
        ATF_REQUIRE( f.matches_test_program(fs::path("dir1/dir2/bar/baz")));
        ATF_REQUIRE( f.matches_test_program(fs::path("dir1/dir2/bar/baz")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("dir1")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("dir1/bar/baz")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("dir2/bar/baz")));
    }

    {
        const cli::test_filter f(fs::path("dir1/dir2"), "unused");
        ATF_REQUIRE( f.matches_test_program(fs::path("dir1/dir2")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("dir1/dir2/foo")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("dir1/dir2/bar")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("dir1/dir2/bar/baz")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("dir1/dir2/bar/baz")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("dir1")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("dir1/bar/baz")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("dir2/bar/baz")));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__matches_test_case)
ATF_TEST_CASE_BODY(test_filter__matches_test_case)
{
    {
        const cli::test_filter f(fs::path("top"), "foo");
        ATF_REQUIRE( f.matches_test_case(mkid("top", "foo")));
        ATF_REQUIRE(!f.matches_test_case(mkid("top", "bar")));
    }

    {
        const cli::test_filter f(fs::path("top"), "");
        ATF_REQUIRE( f.matches_test_case(mkid("top", "foo")));
        ATF_REQUIRE( f.matches_test_case(mkid("top", "bar")));
        ATF_REQUIRE(!f.matches_test_case(mkid("top2", "foo")));
    }

    {
        const cli::test_filter f(fs::path("d1/d2/prog"), "t1");
        ATF_REQUIRE( f.matches_test_case(mkid("d1/d2/prog", "t1")));
        ATF_REQUIRE(!f.matches_test_case(mkid("d1/d2/prog", "t2")));
    }

    {
        const cli::test_filter f(fs::path("d1/d2"), "");
        ATF_REQUIRE( f.matches_test_case(mkid("d1/d2/prog", "t1")));
        ATF_REQUIRE( f.matches_test_case(mkid("d1/d2/prog", "t2")));
        ATF_REQUIRE( f.matches_test_case(mkid("d1/d2/prog2", "t2")));
        ATF_REQUIRE(!f.matches_test_case(mkid("d1/d3", "foo")));
        ATF_REQUIRE(!f.matches_test_case(mkid("d2", "foo")));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__operator_lt)
ATF_TEST_CASE_BODY(test_filter__operator_lt)
{
    {
        const cli::test_filter f1(fs::path("d1/d2"), "");
        ATF_REQUIRE(!(f1 < f1));
    }
    {
        const cli::test_filter f1(fs::path("d1/d2"), "");
        const cli::test_filter f2(fs::path("d1/d3"), "");
        ATF_REQUIRE( (f1 < f2));
        ATF_REQUIRE(!(f2 < f1));
    }
    {
        const cli::test_filter f1(fs::path("d1/d2"), "");
        const cli::test_filter f2(fs::path("d1/d2"), "foo");
        ATF_REQUIRE( (f1 < f2));
        ATF_REQUIRE(!(f2 < f1));
    }
    {
        const cli::test_filter f1(fs::path("d1/d2"), "bar");
        const cli::test_filter f2(fs::path("d1/d2"), "foo");
        ATF_REQUIRE( (f1 < f2));
        ATF_REQUIRE(!(f2 < f1));
    }
    {
        const cli::test_filter f1(fs::path("d1/d2"), "bar");
        const cli::test_filter f2(fs::path("d1/d3"), "");
        ATF_REQUIRE( (f1 < f2));
        ATF_REQUIRE(!(f2 < f1));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__operator_eq)
ATF_TEST_CASE_BODY(test_filter__operator_eq)
{
    const cli::test_filter f1(fs::path("d1/d2"), "");
    const cli::test_filter f2(fs::path("d1/d2"), "bar");
    ATF_REQUIRE( (f1 == f1));
    ATF_REQUIRE(!(f1 == f2));
    ATF_REQUIRE(!(f2 == f1));
    ATF_REQUIRE( (f2 == f2));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__operator_ne)
ATF_TEST_CASE_BODY(test_filter__operator_ne)
{
    const cli::test_filter f1(fs::path("d1/d2"), "");
    const cli::test_filter f2(fs::path("d1/d2"), "bar");
    ATF_REQUIRE(!(f1 != f1));
    ATF_REQUIRE( (f1 != f2));
    ATF_REQUIRE( (f2 != f1));
    ATF_REQUIRE(!(f2 != f2));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__match_test_case__no_filters)
ATF_TEST_CASE_BODY(test_filters__match_test_case__no_filters)
{
    const std::set< cli::test_filter > raw_filters;

    const cli::test_filters filters(raw_filters);
    cli::test_filters::match match;

    match = filters.match_test_case(mkid("foo", "baz"));
    ATF_REQUIRE(match.first);
    ATF_REQUIRE(!match.second);

    match = filters.match_test_case(mkid("foo/bar", "baz"));
    ATF_REQUIRE(match.first);
    ATF_REQUIRE(!match.second);
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__match_test_case__some_filters)
ATF_TEST_CASE_BODY(test_filters__match_test_case__some_filters)
{
    std::set< cli::test_filter > raw_filters;
    raw_filters.insert(mkfilter("top_test", ""));
    raw_filters.insert(mkfilter("subdir_1", ""));
    raw_filters.insert(mkfilter("subdir_2/a_test", ""));
    raw_filters.insert(mkfilter("subdir_2/b_test", "foo"));

    const cli::test_filters filters(raw_filters);
    cli::test_filters::match match;

    match = filters.match_test_case(mkid("top_test", "a"));
    ATF_REQUIRE(match.first);
    ATF_REQUIRE_EQ("top_test", match.second.get().str());

    match = filters.match_test_case(mkid("subdir_1/foo", "a"));
    ATF_REQUIRE(match.first);
    ATF_REQUIRE_EQ("subdir_1", match.second.get().str());

    match = filters.match_test_case(mkid("subdir_1/bar", "z"));
    ATF_REQUIRE(match.first);
    ATF_REQUIRE_EQ("subdir_1", match.second.get().str());

    match = filters.match_test_case(mkid("subdir_2/a_test", "bar"));
    ATF_REQUIRE(match.first);
    ATF_REQUIRE_EQ("subdir_2/a_test", match.second.get().str());

    match = filters.match_test_case(mkid("subdir_2/b_test", "foo"));
    ATF_REQUIRE(match.first);
    ATF_REQUIRE_EQ("subdir_2/b_test:foo", match.second.get().str());

    match = filters.match_test_case(mkid("subdir_2/b_test", "bar"));
    ATF_REQUIRE(!match.first);

    match = filters.match_test_case(mkid("subdir_2/c_test", "foo"));
    ATF_REQUIRE(!match.first);

    match = filters.match_test_case(mkid("subdir_3", "hello"));
    ATF_REQUIRE(!match.first);
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__match_test_program__no_filters)
ATF_TEST_CASE_BODY(test_filters__match_test_program__no_filters)
{
    const std::set< cli::test_filter > raw_filters;

    const cli::test_filters filters(raw_filters);
    ATF_REQUIRE(filters.match_test_program(fs::path("foo")));
    ATF_REQUIRE(filters.match_test_program(fs::path("foo/bar")));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__match_test_program__some_filters)
ATF_TEST_CASE_BODY(test_filters__match_test_program__some_filters)
{
    std::set< cli::test_filter > raw_filters;
    raw_filters.insert(mkfilter("top_test", ""));
    raw_filters.insert(mkfilter("subdir_1", ""));
    raw_filters.insert(mkfilter("subdir_2/a_test", ""));
    raw_filters.insert(mkfilter("subdir_2/b_test", "foo"));

    const cli::test_filters filters(raw_filters);
    ATF_REQUIRE( filters.match_test_program(fs::path("top_test")));
    ATF_REQUIRE( filters.match_test_program(fs::path("subdir_1/foo")));
    ATF_REQUIRE( filters.match_test_program(fs::path("subdir_1/bar")));
    ATF_REQUIRE( filters.match_test_program(fs::path("subdir_2/a_test")));
    ATF_REQUIRE( filters.match_test_program(fs::path("subdir_2/b_test")));
    ATF_REQUIRE(!filters.match_test_program(fs::path("subdir_2/c_test")));
    ATF_REQUIRE(!filters.match_test_program(fs::path("subdir_3")));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__difference__no_filters);
ATF_TEST_CASE_BODY(test_filters__difference__no_filters)
{
    const std::set< cli::test_filter > in_filters;
    const std::set< cli::test_filter > used;
    const std::set< cli::test_filter > diff = cli::test_filters(
        in_filters).difference(used);
    ATF_REQUIRE(diff.empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__difference__some_filters__all_used);
ATF_TEST_CASE_BODY(test_filters__difference__some_filters__all_used)
{
    std::set< cli::test_filter > in_filters;
    in_filters.insert(mkfilter("a", ""));
    in_filters.insert(mkfilter("b", "c"));

    const std::set< cli::test_filter > used = in_filters;

    const std::set< cli::test_filter > diff = cli::test_filters(
        in_filters).difference(used);
    ATF_REQUIRE(diff.empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__difference__some_filters__some_unused);
ATF_TEST_CASE_BODY(test_filters__difference__some_filters__some_unused)
{
    std::set< cli::test_filter > in_filters;
    in_filters.insert(mkfilter("a", ""));
    in_filters.insert(mkfilter("b", "c"));
    in_filters.insert(mkfilter("d", ""));
    in_filters.insert(mkfilter("e", "f"));

    std::set< cli::test_filter > used;
    used.insert(mkfilter("b", "c"));
    used.insert(mkfilter("d", ""));

    const std::set< cli::test_filter > diff = cli::test_filters(
        in_filters).difference(used);
    ATF_REQUIRE_EQ(2, diff.size());
    ATF_REQUIRE(diff.find(mkfilter("a", "")) != diff.end());
    ATF_REQUIRE(diff.find(mkfilter("e", "f")) != diff.end());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_disjoint_filters__ok);
ATF_TEST_CASE_BODY(check_disjoint_filters__ok)
{
    std::set< cli::test_filter > filters;
    filters.insert(mkfilter("a", ""));
    filters.insert(mkfilter("b", ""));
    filters.insert(mkfilter("c", "a"));
    filters.insert(mkfilter("c", "b"));

    cli::check_disjoint_filters(filters);
}


ATF_TEST_CASE_WITHOUT_HEAD(check_disjoint_filters__fail);
ATF_TEST_CASE_BODY(check_disjoint_filters__fail)
{
    std::set< cli::test_filter > filters;
    filters.insert(mkfilter("a", ""));
    filters.insert(mkfilter("b", ""));
    filters.insert(mkfilter("c", "a"));
    filters.insert(mkfilter("d", "b"));
    filters.insert(mkfilter("c", ""));

    ATF_REQUIRE_THROW_RE(std::runtime_error, "'c'.*'c:a'.*not disjoint",
                         cli::check_disjoint_filters(filters));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, test_filter__public_fields);
    ATF_ADD_TEST_CASE(tcs, test_filter__parse__ok);
    ATF_ADD_TEST_CASE(tcs, test_filter__parse__empty);
    ATF_ADD_TEST_CASE(tcs, test_filter__parse__absolute);
    ATF_ADD_TEST_CASE(tcs, test_filter__parse__bad_program_name);
    ATF_ADD_TEST_CASE(tcs, test_filter__parse__bad_test_case);
    ATF_ADD_TEST_CASE(tcs, test_filter__parse__bad_path);
    ATF_ADD_TEST_CASE(tcs, test_filter__str);
    ATF_ADD_TEST_CASE(tcs, test_filter__contains__same);
    ATF_ADD_TEST_CASE(tcs, test_filter__contains__different);
    ATF_ADD_TEST_CASE(tcs, test_filter__matches_test_program);
    ATF_ADD_TEST_CASE(tcs, test_filter__matches_test_case);
    ATF_ADD_TEST_CASE(tcs, test_filter__operator_lt);
    ATF_ADD_TEST_CASE(tcs, test_filter__operator_eq);
    ATF_ADD_TEST_CASE(tcs, test_filter__operator_ne);

    ATF_ADD_TEST_CASE(tcs, test_filters__match_test_case__no_filters);
    ATF_ADD_TEST_CASE(tcs, test_filters__match_test_case__some_filters);
    ATF_ADD_TEST_CASE(tcs, test_filters__match_test_program__no_filters);
    ATF_ADD_TEST_CASE(tcs, test_filters__match_test_program__some_filters);
    ATF_ADD_TEST_CASE(tcs, test_filters__difference__no_filters);
    ATF_ADD_TEST_CASE(tcs, test_filters__difference__some_filters__all_used);
    ATF_ADD_TEST_CASE(tcs, test_filters__difference__some_filters__some_unused);

    ATF_ADD_TEST_CASE(tcs, check_disjoint_filters__ok);
    ATF_ADD_TEST_CASE(tcs, check_disjoint_filters__fail);
}
