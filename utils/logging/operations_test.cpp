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

#include <fstream>
#include <string>

#include <atf-c++.hpp>

#include "utils/datetime.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/operations.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace logging = utils::logging;


ATF_TEST_CASE_WITHOUT_HEAD(generate_log_name__before_log);
ATF_TEST_CASE_BODY(generate_log_name__before_log)
{
    datetime::set_mock_now(2011, 2, 21, 18, 10, 0);
    ATF_REQUIRE_EQ(fs::path("/some/dir/foobar.20110221-181000.log"),
                   logging::generate_log_name(fs::path("/some/dir"), "foobar"));

    datetime::set_mock_now(2011, 2, 21, 18, 10, 1);
    logging::log('I', "file", 123, "A message");

    datetime::set_mock_now(2011, 2, 21, 18, 10, 2);
    ATF_REQUIRE_EQ(fs::path("/some/dir/foobar.20110221-181000.log"),
                   logging::generate_log_name(fs::path("/some/dir"), "foobar"));
}


ATF_TEST_CASE_WITHOUT_HEAD(generate_log_name__after_log);
ATF_TEST_CASE_BODY(generate_log_name__after_log)
{
    datetime::set_mock_now(2011, 2, 21, 18, 15, 0);
    logging::log('I', "file", 123, "A message");
    datetime::set_mock_now(2011, 2, 21, 18, 15, 1);
    logging::log('I', "file", 123, "A message");

    datetime::set_mock_now(2011, 2, 21, 18, 15, 2);
    ATF_REQUIRE_EQ(fs::path("/some/dir/foobar.20110221-181500.log"),
                   logging::generate_log_name(fs::path("/some/dir"), "foobar"));

    datetime::set_mock_now(2011, 2, 21, 18, 15, 3);
    logging::log('I', "file", 123, "A message");

    datetime::set_mock_now(2011, 2, 21, 18, 15, 4);
    ATF_REQUIRE_EQ(fs::path("/some/dir/foobar.20110221-181500.log"),
                   logging::generate_log_name(fs::path("/some/dir"), "foobar"));
}


ATF_TEST_CASE_WITHOUT_HEAD(log);
ATF_TEST_CASE_BODY(log)
{
    datetime::set_mock_now(2011, 2, 21, 18, 10, 0);
    logging::log('D', "f1", 1, "Debug message");

    datetime::set_mock_now(2011, 2, 21, 18, 10, 1);
    logging::log('E', "f2", 2, "Error message");

    logging::set_persistency(fs::path("test.log"));

    datetime::set_mock_now(2011, 2, 21, 18, 10, 2);
    logging::log('I', "f3", 3, "Info message");

    datetime::set_mock_now(2011, 2, 21, 18, 10, 3);
    logging::log('W', "f4", 4, "Warning message");

    std::ifstream input("test.log");
    ATF_REQUIRE(input);

    std::string line;
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ("20110221-181000 D f1:1: Debug message", line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ("20110221-181001 E f2:2: Error message", line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ("20110221-181002 I f3:3: Info message", line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ("20110221-181003 W f4:4: Warning message", line);
}


ATF_TEST_CASE_WITHOUT_HEAD(set_persistency__no_backlog);
ATF_TEST_CASE_BODY(set_persistency__no_backlog)
{
    logging::set_persistency(fs::path("test.log"));

    datetime::set_mock_now(2011, 2, 21, 18, 20, 0);
    logging::log('D', "file", 123, "Debug message");

    std::ifstream input("test.log");
    ATF_REQUIRE(input);

    std::string line;
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ("20110221-182000 D file:123: Debug message", line);
}


ATF_TEST_CASE_WITHOUT_HEAD(set_persistency__some_backlog);
ATF_TEST_CASE_BODY(set_persistency__some_backlog)
{
    datetime::set_mock_now(2011, 2, 21, 18, 20, 0);
    logging::log('D', "file1", 123, "Debug message 1");

    datetime::set_mock_now(2011, 2, 21, 18, 20, 1);
    logging::log('D', "file2", 456, "Debug message 2");

    logging::set_persistency(fs::path("test.log"));

    datetime::set_mock_now(2011, 2, 21, 18, 20, 2);
    logging::log('D', "file3", 789, "Debug message 3");

    std::ifstream input("test.log");
    ATF_REQUIRE(input);

    std::string line;
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ("20110221-182000 D file1:123: Debug message 1", line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ("20110221-182001 D file2:456: Debug message 2", line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ("20110221-182002 D file3:789: Debug message 3", line);
}


ATF_TEST_CASE(set_persistency__fail);
ATF_TEST_CASE_HEAD(set_persistency__fail)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(set_persistency__fail)
{
    fs::mkdir(fs::path("dir"), 0644);
    ATF_REQUIRE_THROW_RE(std::runtime_error, "dir/fail.log",
                         logging::set_persistency(fs::path("dir/fail.log")));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, generate_log_name__before_log);
    ATF_ADD_TEST_CASE(tcs, generate_log_name__after_log);

    ATF_ADD_TEST_CASE(tcs, log);

    ATF_ADD_TEST_CASE(tcs, set_persistency__no_backlog);
    ATF_ADD_TEST_CASE(tcs, set_persistency__some_backlog);
    ATF_ADD_TEST_CASE(tcs, set_persistency__fail);
}
