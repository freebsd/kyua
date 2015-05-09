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

#include <fstream>
#include <stdexcept>

#include "engine/config.hpp"
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
#include "utils/optional.ipp"
#include "utils/passwd.hpp"

namespace config = utils::config;
namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace runner = engine::runner;

using utils::none;
using utils::optional;


namespace {


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
    config::tree _user_config;

    /// Scheduler context to use to load test cases.
    scheduler::scheduler_handle _scheduler_handle;

    /// Constructor.
    impl(const config::tree& user_config_,
         scheduler::scheduler_handle& scheduler_handle_) :
        _loaded(false), _user_config(user_config_),
        _scheduler_handle(scheduler_handle_)
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
/// \param user_config_ User configuration to pass to the scheduler.
/// \param scheduler_handle_ Scheduler context to use to load test cases.
runner::lazy_test_program::lazy_test_program(
    const std::string& interface_name_,
    const fs::path& binary_,
    const fs::path& root_,
    const std::string& test_suite_name_,
    const model::metadata& md_,
    const config::tree& user_config_,
    scheduler::scheduler_handle& scheduler_handle_) :
    test_program(interface_name_, binary_, root_, test_suite_name_, md_,
                 model::test_cases_map()),
    _pimpl(new impl(user_config_, scheduler_handle_))
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
        const model::test_cases_map tcs = _pimpl->_scheduler_handle.list_tests(
            this, _pimpl->_user_config);

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
