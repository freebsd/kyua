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

#include "engine/test_case.hpp"

#include "engine/test_program.hpp"
#include "engine/test_result.hpp"
#include "utils/config/tree.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/optional.ipp"

namespace config = utils::config;
namespace fs = utils::fs;

using utils::none;
using utils::optional;


/// Destructor.
engine::test_case_hooks::~test_case_hooks(void)
{
}


/// Called once the test case's stdout is ready for processing.
///
/// It is important to note that this file is only available within this
/// callback.  Attempting to read the file once the execute function has
/// returned will result in an error because the file might have been deleted.
///
/// \param unused_file The path to the file containing the stdout.
void
engine::test_case_hooks::got_stdout(const fs::path& UTILS_UNUSED_PARAM(file))
{
}


/// Called once the test case's stderr is ready for processing.
///
/// It is important to note that this file is only available within this
/// callback.  Attempting to read the file once the execute function has
/// returned will result in an error because the file might have been deleted.
///
/// \param unused_file The path to the file containing the stderr.
void
engine::test_case_hooks::got_stderr(const fs::path& UTILS_UNUSED_PARAM(file))
{
}


/// Internal implementation for a base_test_case.
struct engine::base_test_case::base_impl {
    /// Test program this test case belongs to.
    const base_test_program& test_program;

    /// Name of the test case; must be unique within the test program.
    std::string name;

    /// Constructor.
    ///
    /// \param test_program_ The test program this test case belongs to.
    /// \param name_ The name of the test case within the test program.
    base_impl(const base_test_program& test_program_,
              const std::string& name_) :
        test_program(test_program_),
        name(name_)
    {
    }
};


/// Constructs a new test case.
///
/// \param test_program_ The test program this test case belongs to.  This is a
///     static reference (instead of a test_program_ptr) because the test
///     program must exist in order for the test case to exist.
/// \param name_ The name of the test case within the test program.  Must be
///     unique.
engine::base_test_case::base_test_case(const base_test_program& test_program_,
                                       const std::string& name_) :
    _pbimpl(new base_impl(test_program_, name_))
{
}


/// Destroys a test case.
engine::base_test_case::~base_test_case(void)
{
}


/// Gets the test program this test case belongs to.
///
/// \return A reference to the container test program.
const engine::base_test_program&
engine::base_test_case::test_program(void) const
{
    return _pbimpl->test_program;
}


/// Gets the test case name.
///
/// \return The test case name, relative to the test program.
const std::string&
engine::base_test_case::name(void) const
{
    return _pbimpl->name;
}


/// Returns a textual description of all metadata properties of this test case.
///
/// This is useful for informative purposes only, as the name of the properties
/// is free form and this abstract class cannot impose any restrictions in them.
///
/// \return A property name to value mapping.
///
/// \todo This probably indicates a bad abstraction.  The 'list' CLI command
/// should maybe just do specific things for every kind of supported test case,
/// instead of having this here.
engine::properties_map
engine::base_test_case::all_properties(void) const
{
    return get_all_properties();
}


/// Runs the test case in debug mode.
///
/// Debug mode gives the caller more control on the execution of the test.  It
/// should not be used for normal execution of tests; instead, call run().
///
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
engine::base_test_case::debug(const config::tree& user_config,
                              test_case_hooks& hooks,
                              const fs::path& stdout_path,
                              const fs::path& stderr_path) const
{
    return execute(user_config, hooks,
                   utils::make_optional(stdout_path),
                   utils::make_optional(stderr_path));
}


/// Runs the test case.
///
/// \param user_config The user configuration that defines the execution of this
///     test case.
/// \param hooks Hooks to introspect the execution of the test case.
///
/// \return The result of the execution of the test case.
engine::test_result
engine::base_test_case::run(const config::tree& user_config,
                            test_case_hooks& hooks) const
{
    return execute(user_config, hooks, none, none);
}
