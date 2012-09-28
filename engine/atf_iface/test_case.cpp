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

#include "engine/atf_iface/test_case.hpp"

#include "engine/atf_iface/runner.hpp"
#include "engine/exceptions.hpp"
#include "engine/metadata.hpp"
#include "engine/test_program.hpp"
#include "engine/test_result.hpp"
#include "utils/config/tree.ipp"
#include "utils/optional.ipp"

namespace atf_iface = engine::atf_iface;
namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;

using utils::none;
using utils::optional;


namespace {


/// Executes the test case.
///
/// This should not throw any exception: problems detected during execution are
/// reported as a broken test case result.
///
/// \param test_case The test case to debug or run.
/// \param user_config The run-time configuration for the test case.
/// \param hooks Hooks to introspect the execution of the test case.
/// \param stdout_path The file to which to redirect the stdout of the test.
///     If none, use a temporary file in the work directory.
/// \param stderr_path The file to which to redirect the stdout of the test.
///     If none, use a temporary file in the work directory.
///
/// \return The result of the execution.
static engine::test_result
execute(const engine::test_case* test_case,
        const config::tree& user_config,
        engine::test_case_hooks& hooks,
        const optional< fs::path >& stdout_path,
        const optional< fs::path >& stderr_path)
{
    return engine::atf_iface::run_test_case(*test_case, user_config, hooks,
                                            stdout_path, stderr_path);
}


}  // anonymous namespace


/// Runs the test case in debug mode.
///
/// Debug mode gives the caller more control on the execution of the test.  It
/// should not be used for normal execution of tests; instead, call run().
///
/// \param test_case The test case to debug.
/// \param user_config The user configuration that defines the execution of this
///     test case.
/// \param hooks Hooks to introspect the execution of the test case.
/// \param stdout_path The file to which to redirect the stdout of the test.
///     For interactive debugging, '/dev/stdout' is probably a reasonable value.
/// \param stderr_path The file to which to redirect the stdout of the test.
///     For interactive debugging, '/dev/stderr' is probably a reasonable value.
///
/// \return The result of the execution of the test case.
engine::test_result
engine::atf_iface::debug_atf_test_case(const test_case* test_case,
                                       const config::tree& user_config,
                                       test_case_hooks& hooks,
                                       const fs::path& stdout_path,
                                       const fs::path& stderr_path)
{
    return execute(test_case, user_config, hooks,
                   utils::make_optional(stdout_path),
                   utils::make_optional(stderr_path));
}


/// Runs the test case.
///
/// \param test_case The test case to run.
/// \param user_config The user configuration that defines the execution of this
///     test case.
/// \param hooks Hooks to introspect the execution of the test case.
///
/// \return The result of the execution of the test case.
engine::test_result
engine::atf_iface::run_atf_test_case(const test_case* test_case,
                                     const config::tree& user_config,
                                     test_case_hooks& hooks)
{
    return execute(test_case, user_config, hooks, none, none);
}
