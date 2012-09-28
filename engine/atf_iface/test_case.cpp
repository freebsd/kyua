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

#include <algorithm>
#include <cstdlib>

#include "engine/atf_iface/runner.hpp"
#include "engine/exceptions.hpp"
#include "engine/metadata.hpp"
#include "engine/test_program.hpp"
#include "engine/test_result.hpp"
#include "utils/config/tree.ipp"
#include "utils/fs/exceptions.hpp"
#include "utils/format/macros.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"
#include "utils/units.hpp"

namespace atf_iface = engine::atf_iface;
namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace units = utils::units;

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
execute(const engine::base_test_case* test_case,
        const config::tree& user_config,
        engine::test_case_hooks& hooks,
        const optional< fs::path >& stdout_path,
        const optional< fs::path >& stderr_path)
{
    const engine::atf_iface::test_case* tc =
        dynamic_cast< const engine::atf_iface::test_case* >(test_case);
    return engine::atf_iface::run_test_case(
        *tc, user_config, hooks, stdout_path, stderr_path);
}


}  // anonymous namespace


/// Constructs a new test case.
///
/// \param test_program_ The test program this test case belongs to.  This
///     object must exist during the lifetime of the test case.
/// \param name_ The name of the test case.
/// \param md_ The test case metadata.
atf_iface::test_case::test_case(const base_test_program& test_program_,
                                const std::string& name_,
                                const metadata& md_) :
    base_test_case("atf", test_program_, name_, md_)
{
}


/// Constructs a new fake test case.
///
/// A fake test case is a test case that is not really defined by the test
/// program.  Such test cases have a name surrounded by '__' and, when executed,
/// they return a fixed, pre-recorded result.  This functionality is used, for
/// example, to dynamically create a test case representing the test program
/// itself when it is broken (i.e. when it's even unable to provide a list of
/// its own test cases).
///
/// \param test_program_ The test program this test case belongs to.
/// \param name_ The name to give to this fake test case.  This name has to be
///     prefixed and suffixed by '__' to clearly denote that this is internal.
/// \param description_ The description of the test case, if any.
/// \param test_result_ The fake result to return when this test case is run.
atf_iface::test_case::test_case(const base_test_program& test_program_,
                                const std::string& name_,
                                const std::string& description_,
                                const engine::test_result& test_result_) :
    base_test_case("atf", test_program_, name_, description_, test_result_)
{
}


/// Destructor.
atf_iface::test_case::~test_case(void)
{
}


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
engine::atf_iface::debug_atf_test_case(const base_test_case* test_case,
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
engine::atf_iface::run_atf_test_case(const base_test_case* test_case,
                                     const config::tree& user_config,
                                     test_case_hooks& hooks)
{
    return execute(test_case, user_config, hooks, none, none);
}
