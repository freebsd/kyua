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
#include "engine/scheduler.hpp"
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
    model::test_cases_map* test_cases =
        *state.to_userdata< model::test_cases_map* >(-1);
    state.pop(1);

    state.push_string("name");
    state.get_table(-2);
    const std::string name = state.to_string(-1);
    state.pop(1);

    model::metadata_builder mdbuilder;

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

    test_cases->insert(model::test_cases_map::value_type(
        name, model::test_case(name, mdbuilder.build())));

    return 0;
}


/// Sets up the Lua state to process the output of a test case list.
///
/// \param [in,out] state The Lua state to configure.
/// \param [out] test_cases Collection to be updated with the loaded test cases.
static void
setup_lua_state(lutok::state& state,
                model::test_cases_map* test_cases)
{
    *state.new_userdata< model::test_cases_map* >() = test_cases;
    state.set_global("_test_cases");

    state.push_cxx_function(lua_test_case);
    state.set_global("test_case");
}


/// Loads the list of test cases from a test program.
///
/// \param interface Name of the interface to use for loading.
/// \param absolute_path Absolute path to the test program.
/// \param props Configuration variables to pass to the test program.
///
/// \return A list of test cases.
static model::test_cases_map
load_test_cases(const std::string& interface,
                const fs::path& absolute_path,
                const config::properties_map& props)
{
    const engine::tester tester(interface, none, none, props);
    const std::string output = tester.list(absolute_path);

    model::test_cases_map test_cases;
    lutok::state state;
    setup_lua_state(state, &test_cases);
    lutok::do_string(state, output, 0, 0, 0);
    return test_cases;
}


/// Creates a tester.
///
/// \param interface_name The name of the tester interface to use.
/// \param metadata Metadata of the test case.
/// \param user_config User-provided configuration variables.
/// \param test_suite Name of the test suite, used to extract the relevant
///     configuration variables from user_config.
///
/// \return The created tester, on which the test() method can be executed.
static engine::tester
create_tester(const std::string& interface_name,
              const model::metadata& metadata, const config::tree& user_config,
              const std::string& test_suite)
{
    optional< passwd::user > user;
    if (user_config.is_set("unprivileged_user") &&
        metadata.required_user() == "unprivileged")
        user = user_config.lookup< engine::user_node >("unprivileged_user");

    config::properties_map props = runner::generate_tester_config(user_config,
                                                                  test_suite);
    // TODO(jmmv): This is an ugly hack to cope with an atf-specific
    // property.  We should not be doing this at all, so just consider this
    // a temporary optimization...
    if (metadata.has_cleanup())
        props["has.cleanup"] = "true";
    else
        props["has.cleanup"] = "false";

    return engine::tester(interface_name, user,
                          utils::make_optional(metadata.timeout()), props);
}


}  // anonymous namespace


/// Internal implementation of a lazy_test_program.
struct engine::runner::lazy_test_program::impl {
    /// Whether the test cases list has been yet loaded or not.
    bool _loaded;

    /// User configuration to pass to the test program list operation.
    config::properties_map _props;

    /// Scheduler context to use to load test cases.
    scheduler::scheduler_handle _scheduler_handle;

    /// Constructor.
    impl(const config::properties_map& props_,
         scheduler::scheduler_handle& scheduler_handle_) :
        _loaded(false), _props(props_), _scheduler_handle(scheduler_handle_)
    {
    }
};


/// Constructs a new test program.
///
/// \param interface_name_ Name of the test program interface.
/// \param binary_ The name of the test program binary relative to root_.
/// \param root_ The root of the test suite containing the test program.
/// \param test_suite_name_ The name of the test suite this program belongs to.
/// \param md_ Metadata of the test program.
/// \param props_ User configuration to pass to the tester.
/// \param scheduler_handle_ Scheduler context to use to load test cases.
runner::lazy_test_program::lazy_test_program(
    const std::string& interface_name_,
    const fs::path& binary_,
    const fs::path& root_,
    const std::string& test_suite_name_,
    const model::metadata& md_,
    const config::properties_map& props_,
    scheduler::scheduler_handle& scheduler_handle_) :
    test_program(interface_name_, binary_, root_, test_suite_name_, md_,
                 model::test_cases_map()),
    _pimpl(new impl(props_, scheduler_handle_))
{
}


/// Gets or loads the list of test cases from the test program.
///
/// \return The list of test cases provided by the test program.
const model::test_cases_map&
runner::lazy_test_program::test_cases(void) const
{
    _pimpl->_scheduler_handle.check_interrupt();

    if (!_pimpl->_loaded) {
        model::test_cases_map tcs;
        try {
            tcs = ::load_test_cases(interface_name(), absolute_path(),
                                    _pimpl->_props);
        } catch (const std::runtime_error& e) {
            // TODO(jmmv): This is a very ugly workaround for the fact that we
            // cannot report failures at the test-program level.  We should
            // either address this, or move this reporting to the testers
            // themselves.
            LW(F("Failed to load test cases list: %s") % e.what());
            model::test_cases_map fake_test_cases;
            fake_test_cases.insert(model::test_cases_map::value_type(
                "__test_cases_list__",
                model::test_case(
                    "__test_cases_list__",
                    "Represents the correct processing of the test cases list",
                    model::test_result(model::test_result_broken, e.what()))));
            tcs = fake_test_cases;
        }

        // Due to the restrictions on when set_test_cases() may be called (as a
        // way to lazily initialize the test cases list before it is ever
        // returned), this cast is valid.
        const_cast< runner::lazy_test_program* >(this)->set_test_cases(tcs);

        _pimpl->_loaded = true;

        _pimpl->_scheduler_handle.check_interrupt();
    }

    INV(_pimpl->_loaded);
    return test_program::test_cases();
}


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


/// Runs the test case in debug mode.
///
/// Debug mode gives the caller more control on the execution of the test.  It
/// should not be used for normal execution of tests; instead, call run().
///
/// \param test_program The test program to debug.
/// \param test_case_name The name of the test case to debug.
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
runner::debug_test_case(const model::test_program* test_program,
                        const std::string& test_case_name,
                        const config::tree& user_config,
                        test_case_hooks& hooks,
                        const fs::path& work_directory,
                        const fs::path& stdout_path,
                        const fs::path& stderr_path)
{
    const model::test_case& test_case = test_program->find(test_case_name);

    if (test_case.fake_result())
        return test_case.fake_result().get();

    const std::string skip_reason = check_reqs(
        test_case.get_metadata(), user_config, test_program->test_suite_name(),
        work_directory);
    if (!skip_reason.empty())
        return model::test_result(model::test_result_skipped, skip_reason);

    if (!fs::exists(test_program->absolute_path()))
        return model::test_result(model::test_result_broken,
                                  "Test program does not exist");

    const fs::auto_file result_file(work_directory / "result.txt");

    try {
        const engine::tester tester = create_tester(
            test_program->interface_name(), test_case.get_metadata(),
            user_config, test_program->test_suite_name());
        tester.test(test_program->absolute_path(), test_case.name(),
                    result_file.file(), stdout_path, stderr_path);

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
            model::test_result_broken,
            F("Caught unexpected exception: %s") % e.what());
    }
}


/// Generates the set of configuration variables for the tester.
///
/// \param user_config The configuration variables provided by the user.
/// \param test_suite The name of the test suite.
///
/// \return The mapping of configuration variables for the tester.
config::properties_map
runner::generate_tester_config(const config::tree& user_config,
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

    return props;
}


/// Runs the test case.
///
/// \param test_program The test program to run.
/// \param test_case_name The name of the test case to run.
/// \param user_config The user configuration that defines the execution of this
///     test case.
/// \param hooks Hooks to introspect the execution of the test case.
/// \param work_directory A directory that can be used to place temporary files.
///
/// \return The result of the execution of the test case.
model::test_result
runner::run_test_case(const model::test_program* test_program,
                      const std::string& test_case_name,
                      const config::tree& user_config,
                      test_case_hooks& hooks,
                      const fs::path& work_directory)
{
    const model::test_case& test_case = test_program->find(test_case_name);

    if (test_case.fake_result())
        return test_case.fake_result().get();

    const std::string skip_reason = check_reqs(
        test_case.get_metadata(), user_config, test_program->test_suite_name(),
        work_directory);
    if (!skip_reason.empty())
        return model::test_result(model::test_result_skipped, skip_reason);

    if (!fs::exists(test_program->absolute_path()))
        return model::test_result(model::test_result_broken,
                                  "Test program does not exist");

    const fs::auto_file stdout_file(work_directory / "stdout.txt");
    const fs::auto_file stderr_file(work_directory / "stderr.txt");
    const fs::auto_file result_file(work_directory / "result.txt");

    try {
        const engine::tester tester = create_tester(
            test_program->interface_name(), test_case.get_metadata(),
            user_config, test_program->test_suite_name());
        tester.test(test_program->absolute_path(), test_case.name(),
                    result_file.file(), stdout_file.file(), stderr_file.file());

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
            model::test_result_broken,
            F("Caught unexpected exception: %s") % e.what());
    }
}
