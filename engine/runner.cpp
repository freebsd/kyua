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

#include "engine/config.hpp"
#include "engine/scheduler.hpp"
#include "model/context.hpp"
#include "model/metadata_fwd.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "utils/config/tree.ipp"
#include "utils/defs.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/passwd.hpp"

namespace config = utils::config;
namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace runner = engine::runner;


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


/// Queries the current execution context.
///
/// \return The queried context.
model::context
runner::current_context(void)
{
    return model::context(fs::current_path(), utils::getallenv());
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
