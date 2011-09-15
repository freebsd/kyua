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
#include <lutok/operations.hpp>
#include <lutok/test_utils.hpp>
#include <lutok/wrap.hpp>

#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/lua_module.hpp"
#include "utils/logging/operations.hpp"

namespace fs = utils::fs;
namespace logging = utils::logging;


/// Ensures that a particular logging.<type> function works.
///
/// \param exp_type The expected type of the resulting error message.
/// \param function The name of the logging.<type> function.
static void
do_logging_ok_check(const char exp_type, const char* function)
{
    lutok::state state;
    logging::open_logging(state);

    logging::set_persistency("debug", fs::path("test.log"));
    std::ofstream output("test.lua");
    output << F("\n%s('The message from lua!')\n") % function;
    output.close();
    lutok::do_file(state, "test.lua");

    std::ifstream input("test.log");
    ATF_REQUIRE(input);

    std::string line;
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_MATCH(F(" %c .*test.lua:2: .*The message from lua!") % exp_type,
                      line);
}


/// Ensures that a particular logging.<type> function detects invalid arguments.
///
/// \param function The name of the logging.<type> function.
static void
do_logging_fail_check(const char* function)
{
    lutok::state state;
    logging::open_logging(state);

    ATF_REQUIRE_THROW_RE(lutok::error, "message must be a string",
                         lutok::do_string(state, F("%s({})\n") % function));
}


ATF_TEST_CASE_WITHOUT_HEAD(open_logging);
ATF_TEST_CASE_BODY(open_logging)
{
    lutok::state state;
    stack_balance_checker checker(state);
    logging::open_logging(state);
    lutok::do_string(state, "return logging.error", 1);
    ATF_REQUIRE(state.is_function());
    lutok::do_string(state, "return logging.warning", 1);
    ATF_REQUIRE(state.is_function());
    lutok::do_string(state, "return logging.info", 1);
    ATF_REQUIRE(state.is_function());
    lutok::do_string(state, "return logging.debug", 1);
    ATF_REQUIRE(state.is_function());
    state.pop(4);
}


ATF_TEST_CASE_WITHOUT_HEAD(logging__error__ok);
ATF_TEST_CASE_BODY(logging__error__ok)
{
    do_logging_ok_check('E', "logging.error");
}


ATF_TEST_CASE_WITHOUT_HEAD(logging__error__fail);
ATF_TEST_CASE_BODY(logging__error__fail)
{
    do_logging_fail_check("logging.error");
}


ATF_TEST_CASE_WITHOUT_HEAD(logging__warning__ok);
ATF_TEST_CASE_BODY(logging__warning__ok)
{
    do_logging_ok_check('W', "logging.warning");
}


ATF_TEST_CASE_WITHOUT_HEAD(logging__warning__fail);
ATF_TEST_CASE_BODY(logging__warning__fail)
{
    do_logging_fail_check("logging.warning");
}


ATF_TEST_CASE_WITHOUT_HEAD(logging__info__ok);
ATF_TEST_CASE_BODY(logging__info__ok)
{
    do_logging_ok_check('I', "logging.info");
}


ATF_TEST_CASE_WITHOUT_HEAD(logging__info__fail);
ATF_TEST_CASE_BODY(logging__info__fail)
{
    do_logging_fail_check("logging.info");
}


ATF_TEST_CASE_WITHOUT_HEAD(logging__debug__ok);
ATF_TEST_CASE_BODY(logging__debug__ok)
{
    do_logging_ok_check('D', "logging.debug");
}


ATF_TEST_CASE_WITHOUT_HEAD(logging__debug__fail);
ATF_TEST_CASE_BODY(logging__debug__fail)
{
    do_logging_fail_check("logging.debug");
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, open_logging);

    ATF_ADD_TEST_CASE(tcs, logging__error__ok);
    ATF_ADD_TEST_CASE(tcs, logging__error__fail);
    ATF_ADD_TEST_CASE(tcs, logging__warning__ok);
    ATF_ADD_TEST_CASE(tcs, logging__warning__fail);
    ATF_ADD_TEST_CASE(tcs, logging__info__ok);
    ATF_ADD_TEST_CASE(tcs, logging__info__fail);
    ATF_ADD_TEST_CASE(tcs, logging__debug__ok);
    ATF_ADD_TEST_CASE(tcs, logging__debug__fail);
}
