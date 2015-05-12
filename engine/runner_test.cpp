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

#include "engine/runner.hpp"

#include <atf-c++.hpp>

#include "engine/config.hpp"
#include "model/context.hpp"
#include "utils/config/tree.ipp"
#include "utils/env.hpp"
#include "utils/format/containers.ipp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/passwd.hpp"

namespace config = utils::config;
namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace runner = engine::runner;


ATF_TEST_CASE_WITHOUT_HEAD(current_context);
ATF_TEST_CASE_BODY(current_context)
{
    const model::context context = runner::current_context();
    ATF_REQUIRE_EQ(fs::current_path(), context.cwd());
    ATF_REQUIRE(utils::getallenv() == context.env());
}


ATF_TEST_CASE_WITHOUT_HEAD(generate_tester_config__empty);
ATF_TEST_CASE_BODY(generate_tester_config__empty)
{
    const config::tree user_config = engine::empty_config();

    const config::properties_map exp_props;

    ATF_REQUIRE_EQ(exp_props,
                   runner::generate_tester_config(user_config, "missing"));
}


ATF_TEST_CASE_WITHOUT_HEAD(generate_tester_config__no_matches);
ATF_TEST_CASE_BODY(generate_tester_config__no_matches)
{
    config::tree user_config = engine::empty_config();
    user_config.set_string("architecture", "foo");
    user_config.set_string("test_suites.one.var1", "value 1");

    const config::properties_map exp_props;

    ATF_REQUIRE_EQ(exp_props,
                   runner::generate_tester_config(user_config, "two"));
}


ATF_TEST_CASE_WITHOUT_HEAD(generate_tester_config__some_matches);
ATF_TEST_CASE_BODY(generate_tester_config__some_matches)
{
    std::vector< passwd::user > mock_users;
    mock_users.push_back(passwd::user("nobody", 1234, 5678));
    passwd::set_mock_users_for_testing(mock_users);

    config::tree user_config = engine::empty_config();
    user_config.set_string("architecture", "foo");
    user_config.set_string("unprivileged_user", "nobody");
    user_config.set_string("test_suites.one.var1", "value 1");
    user_config.set_string("test_suites.two.var2", "value 2");

    config::properties_map exp_props;
    exp_props["unprivileged-user"] = "nobody";
    exp_props["var1"] = "value 1";

    ATF_REQUIRE_EQ(exp_props,
                   runner::generate_tester_config(user_config, "one"));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, current_context);

    ATF_ADD_TEST_CASE(tcs, generate_tester_config__empty);
    ATF_ADD_TEST_CASE(tcs, generate_tester_config__no_matches);
    ATF_ADD_TEST_CASE(tcs, generate_tester_config__some_matches);
}
