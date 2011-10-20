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

extern "C" {
#include <sys/stat.h>

#include <unistd.h>
}

#include <fstream>
#include <map>
#include <set>

#include <atf-c++.hpp>

#include "cli/cmd_list.hpp"
#include "cli/common.ipp"
// TODO(jmmv): Should probably use a mock test case.
#include "engine/atf_iface/test_case.hpp"
// TODO(jmmv): Should probably use a mock test program.
#include "engine/atf_iface/test_program.hpp"
#include "engine/drivers/list_tests.hpp"
#include "engine/exceptions.hpp"
#include "engine/filters.hpp"
#include "engine/user_files/kyuafile.hpp"
#include "utils/env.hpp"

namespace atf_iface = engine::atf_iface;
namespace list_tests = engine::drivers::list_tests;
namespace user_files = engine::user_files;
namespace fs = utils::fs;


namespace {


/// Gets the path to the helpers for this test program.
///
/// \param tc A pointer to the currently running test case.
///
/// \return The path to the helpers binary.
static fs::path
helpers(const atf::tests::tc* test_case)
{
    return fs::path(test_case->get_config_var("srcdir")) /
        "list_tests_helpers";
}


class capture_hooks : public list_tests::base_hooks {
public:
    std::map< std::string, std::string > bogus_test_programs;
    std::set< std::string > test_cases;

    virtual void
    got_bogus_test_program(const engine::base_test_program& test_program,
                           const std::string& reason)
    {
        bogus_test_programs[test_program.relative_path().str()] = reason;
    }

    virtual void
    got_test_case(const engine::base_test_case& test_case)
    {
        test_cases.insert(test_case.identifier().str());
    }
};


}  // anonymous namespace


static list_tests::result
run_helpers(const atf::tests::tc* tc, list_tests::base_hooks& hooks,
            const char* filter_program = NULL,
            const char* filter_test_case = NULL)
{
    ATF_REQUIRE(::mkdir("root", 0755) != -1);
    ATF_REQUIRE(::mkdir("root/dir", 0755) != -1);
    ATF_REQUIRE(::symlink(helpers(tc).c_str(), "root/dir/program") != -1);

    std::ofstream kyuafile1("root/Kyuafile");
    kyuafile1 << "syntax('kyuafile', 1)\n";
    kyuafile1 << "include('dir/Kyuafile')\n";
    kyuafile1.close();

    std::ofstream kyuafile2("root/dir/Kyuafile");
    kyuafile2 << "syntax('kyuafile', 1)\n";
    kyuafile2 << "atf_test_program{name='program', test_suite='suite-name'}\n";
    kyuafile2.close();

    std::set< engine::test_filter > filters;
    if (filter_program != NULL && filter_test_case != NULL)
        filters.insert(engine::test_filter(fs::path(filter_program),
                                           filter_test_case));

    return list_tests::drive(fs::path("root/Kyuafile"), filters, hooks);
}


ATF_TEST_CASE_WITHOUT_HEAD(one_test_case);
ATF_TEST_CASE_BODY(one_test_case)
{
    utils::setenv("TESTS", "some_properties");
    capture_hooks hooks;
    run_helpers(this, hooks);

    std::set< std::string > exp_test_cases;
    exp_test_cases.insert("dir/program:some_properties");
    ATF_REQUIRE(exp_test_cases == hooks.test_cases);
    ATF_REQUIRE(hooks.bogus_test_programs.empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(many_test_cases);
ATF_TEST_CASE_BODY(many_test_cases)
{
    utils::setenv("TESTS", "no_properties some_properties");
    capture_hooks hooks;
    run_helpers(this, hooks);

    std::set< std::string > exp_test_cases;
    exp_test_cases.insert("dir/program:no_properties");
    exp_test_cases.insert("dir/program:some_properties");
    ATF_REQUIRE(exp_test_cases == hooks.test_cases);
    ATF_REQUIRE(hooks.bogus_test_programs.empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(filter_match);
ATF_TEST_CASE_BODY(filter_match)
{
    utils::setenv("TESTS", "no_properties some_properties");
    capture_hooks hooks;
    run_helpers(this, hooks, "dir/program", "some_properties");

    std::set< std::string > exp_test_cases;
    exp_test_cases.insert("dir/program:some_properties");
    ATF_REQUIRE(exp_test_cases == hooks.test_cases);
    ATF_REQUIRE(hooks.bogus_test_programs.empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(crash);
ATF_TEST_CASE_BODY(crash)
{
    utils::setenv("TESTS", "crash_list");
    capture_hooks hooks;
    run_helpers(this, hooks, "dir/program", "some_properties");

    std::map< std::string, std::string > exp_bogus_test_programs;
    exp_bogus_test_programs["dir/program"] = "Test program did not exit "
        "cleanly";
    ATF_REQUIRE(hooks.test_cases.empty());
    ATF_REQUIRE(exp_bogus_test_programs == hooks.bogus_test_programs);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, one_test_case);
    ATF_ADD_TEST_CASE(tcs, many_test_cases);
    ATF_ADD_TEST_CASE(tcs, filter_match);
    ATF_ADD_TEST_CASE(tcs, crash);
}
