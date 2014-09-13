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

extern "C" {
#include <signal.h>
}

#include <fstream>
#include <stdexcept>

#include <lutok/operations.hpp>
#include <lutok/state.ipp>

#include "engine/config.hpp"
#include "engine/exceptions.hpp"
#include "engine/requirements.hpp"
#include "engine/testers.hpp"
#include "model/context.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "utils/config/tree.ipp"
#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/auto_cleaners.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/logging/operations.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"
#include "utils/text/operations.ipp"

namespace config = utils::config;
namespace fs = utils::fs;
namespace logging = utils::logging;
namespace passwd = utils::passwd;
namespace runner = engine::runner;
namespace text = utils::text;

using utils::none;
using utils::optional;


namespace {


/// Lua hook for the test_case function.
///
/// \pre state(-1) contains the arguments to the function.
///
/// \param state The Lua state in which we are running.
///
/// \return The number of return values, which is always 0.
static int
lua_test_case(lutok::state& state)
{
    if (!state.is_table(-1))
        throw std::runtime_error("Oh noes"); // XXX

    state.get_global("_test_cases");
    model::test_cases_vector* test_cases =
        *state.to_userdata< model::test_cases_vector* >(-1);
    state.pop(1);

    state.get_global("_test_program");
    const model::test_program* test_program =
        *state.to_userdata< model::test_program* >(-1);
    state.pop(1);

    state.push_string("name");
    state.get_table(-2);
    const std::string name = state.to_string(-1);
    state.pop(1);

    model::metadata_builder mdbuilder(test_program->get_metadata());

    state.push_nil();
    while (state.next(-2)) {
        if (!state.is_string(-2))
            throw std::runtime_error("Oh oh");  // XXX
        const std::string property = state.to_string(-2);

        if (!state.is_string(-1))
            throw std::runtime_error("Oh oh");  // XXX
        const std::string value = state.to_string(-1);

        if (property != "name")
            mdbuilder.set_string(property, value);

        state.pop(1);
    }
    state.pop(1);

    model::test_case_ptr test_case(
        new model::test_case(test_program->interface_name(), *test_program,
                             name, mdbuilder.build()));
    test_cases->push_back(test_case);

    return 0;
}


/// Sets up the Lua state to process the output of a test case list.
///
/// \param [in,out] state The Lua state to configure.
/// \param test_program Pointer to the test program being loaded.
/// \param [out] test_cases Vector that will contain the list of test cases.
static void
setup_lua_state(lutok::state& state,
                const model::test_program* test_program,
                model::test_cases_vector* test_cases)
{
    *state.new_userdata< model::test_cases_vector* >() = test_cases;
    state.set_global("_test_cases");

    *state.new_userdata< const model::test_program* >() = test_program;
    state.set_global("_test_program");

    state.push_cxx_function(lua_test_case);
    state.set_global("test_case");
}


/// Loads the list of test cases from a test program.
///
/// \param test_program Representation of the test program to load.
///
/// \return A list of test cases.
static model::test_cases_vector
load_test_cases(const model::test_program& test_program)
{
    const engine::tester tester(test_program.interface_name(), none, none);
    const std::string output = tester.list(test_program.absolute_path());

    model::test_cases_vector test_cases;
    lutok::state state;
    setup_lua_state(state, &test_program, &test_cases);
    lutok::do_string(state, output, 0, 0, 0);
    return test_cases;
}


/// Generates the set of configuration variables for the tester.
///
/// \param metadata The metadata of the test.
/// \param user_config The configuration variables provided by the user.
/// \param test_suite The name of the test suite.
///
/// \return The mapping of configuration variables for the tester.
static config::properties_map
generate_tester_config(const model::metadata& metadata,
                       const config::tree& user_config,
                       const std::string& test_suite)
{
    config::properties_map props;

    try {
        props = user_config.all_properties(F("test_suites.%s") % test_suite,
                                           true);
    } catch (const config::unknown_key_error& unused_error) {
        // Ignore: not all test suites have entries in the configuration.
    }

    if (user_config.is_set("unprivileged_user")) {
        const passwd::user& user =
            user_config.lookup< engine::user_node >("unprivileged_user");
        props["unprivileged-user"] = user.name;
    }

    // TODO(jmmv): This is an ugly hack to cope with an atf-specific
    // property.  We should not be doing this at all, so just consider this
    // a temporary optimization...
    if (metadata.has_cleanup())
        props["has.cleanup"] = "true";
    else
        props["has.cleanup"] = "false";

    return props;
}


/// Creates a tester.
///
/// \param interface_name The name of the tester interface to use.
/// \param metadata Metadata of the test case.
/// \param user_config User-provided configuration variables.
///
/// \return The created tester, on which the test() method can be executed.
static engine::tester
create_tester(const std::string& interface_name,
              const model::metadata& metadata, const config::tree& user_config)
{
    optional< passwd::user > user;
    if (user_config.is_set("unprivileged_user") &&
        metadata.required_user() == "unprivileged")
        user = user_config.lookup< engine::user_node >("unprivileged_user");

    return engine::tester(interface_name, user,
                          utils::make_optional(metadata.timeout()));
}


}  // anonymous namespace


/// Destructor.
runner::test_case_hooks::~test_case_hooks(void)
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
runner::test_case_hooks::got_stdout(const fs::path& UTILS_UNUSED_PARAM(file))
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
runner::test_case_hooks::got_stderr(const fs::path& UTILS_UNUSED_PARAM(file))
{
}


/// Queries the current execution context.
///
/// \return The queried context.
model::context
runner::current_context(void)
{
    return model::context(fs::current_path(), utils::getallenv());
}


/// Gets the list of test cases from the test program.
///
/// \param [in,out] program The test program to update with the loaded list of
///     test cases.
void
runner::load_test_cases(model::test_program& program)
{
    if (!program.has_test_cases()) {
        model::test_cases_vector test_cases;
        try {
            test_cases = ::load_test_cases(program);
        } catch (const std::runtime_error& e) {
            // TODO(jmmv): This is a very ugly workaround for the fact that we
            // cannot report failures at the test-program level.  We should
            // either address this, or move this reporting to the testers
            // themselves.
            LW(F("Failed to load test cases list: %s") % e.what());
            model::test_cases_vector fake_test_cases;
            fake_test_cases.push_back(model::test_case_ptr(new model::test_case(
                program.interface_name(), program, "__test_cases_list__",
                "Represents the correct processing of the test cases list",
                model::test_result(model::test_result::broken, e.what()))));
            test_cases = fake_test_cases;
        }
        program.set_test_cases(test_cases);
    }
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
/// \param work_directory A directory that can be used to place temporary files.
/// \param stdout_path The file to which to redirect the stdout of the test.
///     For interactive debugging, '/dev/stdout' is probably a reasonable value.
/// \param stderr_path The file to which to redirect the stdout of the test.
///     For interactive debugging, '/dev/stderr' is probably a reasonable value.
///
/// \return The result of the execution of the test case.
model::test_result
runner::debug_test_case(const model::test_case* test_case,
                        const config::tree& user_config,
                        test_case_hooks& hooks,
                        const fs::path& work_directory,
                        const fs::path& stdout_path,
                        const fs::path& stderr_path)
{
    if (test_case->fake_result())
        return test_case->fake_result().get();

    const std::string skip_reason = check_reqs(
        test_case->get_metadata(), user_config,
        test_case->container_test_program().test_suite_name());
    if (!skip_reason.empty())
        return model::test_result(model::test_result::skipped, skip_reason);

    if (!fs::exists(test_case->container_test_program().absolute_path()))
        return model::test_result(model::test_result::broken,
                                  "Test program does not exist");

    const fs::auto_file result_file(work_directory / "result.txt");

    const model::test_program& test_program =
        test_case->container_test_program();

    try {
        const engine::tester tester = create_tester(
            test_program.interface_name(), test_case->get_metadata(),
            user_config);
        tester.test(test_program.absolute_path(), test_case->name(),
                    result_file.file(), stdout_path, stderr_path,
                    generate_tester_config(test_case->get_metadata(),
                                           user_config,
                                           test_program.test_suite_name()));

        hooks.got_stdout(stdout_path);
        hooks.got_stderr(stderr_path);

        std::ifstream result_input(result_file.file().c_str());
        return parse_test_result(result_input);
    } catch (const std::runtime_error& e) {
        // One of the possible explanation for us getting here is if the tester
        // crashes or doesn't behave as expected.  We must record any output
        // from the process so that we can debug it further.
        hooks.got_stdout(stdout_path);
        hooks.got_stderr(stderr_path);

        return model::test_result(
            model::test_result::broken,
            F("Caught unexpected exception: %s") % e.what());
    }
}


/// Runs the test case.
///
/// \param test_case The test case to run.
/// \param user_config The user configuration that defines the execution of this
///     test case.
/// \param hooks Hooks to introspect the execution of the test case.
/// \param work_directory A directory that can be used to place temporary files.
///
/// \return The result of the execution of the test case.
model::test_result
runner::run_test_case(const model::test_case* test_case,
                      const config::tree& user_config,
                      test_case_hooks& hooks,
                      const fs::path& work_directory)
{
    if (test_case->fake_result())
        return test_case->fake_result().get();

    const std::string skip_reason = check_reqs(
        test_case->get_metadata(), user_config,
        test_case->container_test_program().test_suite_name());
    if (!skip_reason.empty())
        return model::test_result(model::test_result::skipped, skip_reason);

    if (!fs::exists(test_case->container_test_program().absolute_path()))
        return model::test_result(model::test_result::broken,
                                  "Test program does not exist");

    const fs::auto_file stdout_file(work_directory / "stdout.txt");
    const fs::auto_file stderr_file(work_directory / "stderr.txt");
    const fs::auto_file result_file(work_directory / "result.txt");

    const model::test_program& test_program =
        test_case->container_test_program();

    try {
        const engine::tester tester = create_tester(
            test_program.interface_name(), test_case->get_metadata(),
            user_config);
        tester.test(test_program.absolute_path(), test_case->name(),
                    result_file.file(), stdout_file.file(), stderr_file.file(),
                    generate_tester_config(test_case->get_metadata(),
                                           user_config,
                                           test_program.test_suite_name()));

        hooks.got_stdout(stdout_file.file());
        hooks.got_stderr(stderr_file.file());

        std::ifstream result_input(result_file.file().c_str());
        return parse_test_result(result_input);
    } catch (const std::runtime_error& e) {
        // One of the possible explanation for us getting here is if the tester
        // crashes or doesn't behave as expected.  We must record any output
        // from the process so that we can debug it further.
        hooks.got_stdout(stdout_file.file());
        hooks.got_stderr(stderr_file.file());

        return model::test_result(
            model::test_result::broken,
            F("Caught unexpected exception: %s") % e.what());
    }
}
