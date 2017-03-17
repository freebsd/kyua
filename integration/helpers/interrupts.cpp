// Copyright 2011 The Kyua Authors.
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
#include <unistd.h>
}

#include <atf-c++.hpp>

#include <fstream>

#include "utils/env.hpp"
#include "utils/optional.ipp"
#include "utils/logging/operations.hpp"

namespace logging = utils::logging;

using utils::optional;


/// Creates an empty file.
///
/// \param path The file to create.
static void
create_cookie(const std::string& path)
{
    std::ofstream output(path.c_str());
    output.close();
}


ATF_TEST_CASE_WITH_CLEANUP(block_body);
ATF_TEST_CASE_HEAD(block_body)
{
    set_md_var("require.config", "body-cookie cleanup-cookie");
}
ATF_TEST_CASE_BODY(block_body)
{
    create_cookie(get_config_var("body-cookie"));
    for (;;)
        ::pause();
}
ATF_TEST_CASE_CLEANUP(block_body)
{
    create_cookie(get_config_var("cleanup-cookie"));
}


ATF_TEST_CASE_WITH_CLEANUP(block_cleanup);
ATF_TEST_CASE_HEAD(block_cleanup)
{
    set_md_var("require.config",
               "body-cookie cleanup-pre-cookie cleanup-post-cookie");
}
ATF_TEST_CASE_BODY(block_cleanup)
{
    create_cookie(get_config_var("body-cookie"));
    for (;;)
        ::pause();
}
ATF_TEST_CASE_CLEANUP(block_cleanup)
{
    create_cookie(get_config_var("cleanup-pre-cookie"));
    // Sleep instead of block.  If the signal handling code fails to kill the
    // cleanup routine, we want the test to detect it later.
    ::sleep(60);
    create_cookie(get_config_var("cleanup-post-cookie"));
}


ATF_INIT_TEST_CASES(tcs)
{
    logging::set_inmemory();
    const optional< std::string > test_case = utils::getenv("TEST_CASE");
    if (!test_case)
        std::abort();
    if (test_case.get() == "block_body") {
        ATF_ADD_TEST_CASE(tcs, block_body);
    } else if (test_case.get() == "block_cleanup") {
        ATF_ADD_TEST_CASE(tcs, block_cleanup);
    } else
        create_cookie("/tmp/oh");
}
