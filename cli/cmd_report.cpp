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

#include <cstddef>
#include <cstdlib>
#include <map>
#include <vector>

#include "cli/cmd_report.hpp"
#include "cli/common.ipp"
#include "engine/action.hpp"
#include "engine/context.hpp"
#include "engine/drivers/scan_action.hpp"
#include "engine/test_result.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/optional.ipp"

namespace cmdline = utils::cmdline;
namespace fs = utils::fs;
namespace scan_action = engine::drivers::scan_action;
namespace user_files = engine::user_files;

using cli::cmd_report;
using utils::optional;


namespace {


/// Generates a plain-text report intended to be printed to the console.
class console_hooks : public scan_action::base_hooks {
    /// The user interface to which to send the output.
    cmdline::ui* const _ui;

    /// Whether to include the runtime context in the output or not.
    const bool _show_context;

    /// The action ID loaded.
    int64_t _action_id;

    /// The amount of results received.
    ///
    /// We have to maintain this information aside _results, because _results
    /// does not include passed tests.
    std::size_t _total;

    /// Representation of a single result.
    struct result_data {
        /// The relative path to the test program.
        fs::path binary_path;

        /// The name of the test case.
        std::string test_case_name;

        /// The result of the test case.
        engine::test_result result;

        /// Constructs a new results data.
        ///
        /// \param binary_path_ The relative path to the test program.
        /// \param test_case_name_ The name of the test case.
        /// \param result_ The result of the test case.
        result_data(const fs::path& binary_path_,
                    const std::string& test_case_name_,
                    const engine::test_result& result_) :
            binary_path(binary_path_), test_case_name(test_case_name_),
            result(result_)
        {
        }
    };

    /// Results received, broken down by their type.
    ///
    /// Note that this may not include all results, as keeping the whole list in
    /// memory may be too much.
    std::map< engine::test_result::result_type,
              std::vector< result_data > > _results;

    /// Prints the execution context to the output.
    ///
    /// \param context The context to dump.
    void
    print_context(const engine::context& context) const
    {
        _ui->out("===> Execution context");

        _ui->out(F("Current directory: %s") % context.cwd());
        const std::map< std::string, std::string >& env = context.env();
        if (env.empty())
            _ui->out("No environment variables recorded");
        else {
            _ui->out("Environment variables:");
            for (std::map< std::string, std::string >::const_iterator
                     iter = env.begin(); iter != env.end(); iter++) {
                _ui->out(F("    %s=%s") % (*iter).first % (*iter).second);
            }
        }
    }

    /// Prints a set of results.
    std::size_t
    print_results(const engine::test_result::result_type type,
                  const char* title) const
    {
        const std::map< engine::test_result::result_type,
                        std::vector< result_data > >::const_iterator iter2 =
            _results.find(type);
        if (iter2 == _results.end())
            return 0;
        const std::vector< result_data >& all = (*iter2).second;

        _ui->out(F("===> %s") % title);
        for (std::vector< result_data >::const_iterator iter = all.begin();
             iter != all.end(); iter++) {
            _ui->out(F("%s:%s  ->  %s") % (*iter).binary_path %
                     (*iter).test_case_name %
                     cli::format_result((*iter).result));
        }
        return all.size();
    }

public:
    /// Constructor for the hooks.
    ///
    /// \param ui_ The user interface to which to send the output.
    /// \param show_context_ Whether to include the runtime context in
    ///     the output or not.
    console_hooks(cmdline::ui* ui_, bool show_context_) :
        _ui(ui_),
        _show_context(show_context_),
        _total(0)
    {
    }

    /// Callback executed when an action is found.
    ///
    /// \param action_id The identifier of the loaded action.
    /// \param action The action loaded from the database.
    void
    got_action(const int64_t action_id, const engine::action& action)
    {
        _action_id = action_id;
        if (_show_context)
            print_context(action.runtime_context());
    }

    /// Callback executed when a test results is found.
    ///
    /// \param test_program The test program the result belongs to.
    /// \param test_case_name The name of the test case.
    /// \param result The result of the test case.
    void
    got_result(const engine::test_program_ptr& test_program,
               const std::string& test_case_name,
               const engine::test_result& result)
    {
        ++_total;
        if (result.type() != engine::test_result::passed)
            _results[result.type()].push_back(
                result_data(test_program->relative_path(), test_case_name,
                            result));
    }

    /// Prints the tests summary.
    void
    print_tests(void) const
    {
        if (_total == 0)
            return;

        const std::size_t skipped = print_results(
            engine::test_result::skipped, "Skipped tests");
        const std::size_t xfail = print_results(
            engine::test_result::expected_failure, "Expected failures");
        const std::size_t broken = print_results(
            engine::test_result::broken, "Broken tests");
        const std::size_t failed = print_results(
            engine::test_result::failed, "Failed tests");

        _ui->out("===> Summary");
        _ui->out(F("Action: %d") % _action_id);
        _ui->out(F("Test cases: %d total, %d skipped, %d expected failures, "
                   "%d broken, %d failed") %
                 _total % skipped % xfail % broken % failed);
    }
};


}  // anonymous namespace


/// Default constructor for cmd_report.
cmd_report::cmd_report(void) : cli_command(
    "report", "", 0, 0,
    "Generates a report with the result of a previous action")
{
    add_option(store_option);
    add_option(cmdline::bool_option(
        "show-context", "Include the execution context in the report"));
    add_option(cmdline::int_option(
        "action", "The action to report; if not specified, defaults to the "
        "latest action in the database", "id"));
    add_option(cmdline::string_option(
        'o', "output", "The format of the output and the location for the "
        "output", "format:output", "console:/dev/stdout"));
}


/// Entry point for the "report" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
/// \param unused_config The runtime configuration of the program.
///
/// \return 0 if everything is OK, 1 if the statement is invalid or if there is
/// any other problem.
int
cmd_report::run(cmdline::ui* ui, const cmdline::parsed_cmdline& cmdline,
                const user_files::config& UTILS_UNUSED_PARAM(config))
{
    if (cmdline.get_option< cmdline::string_option >("output") !=
        "console:/dev/stdout") {
        throw cmdline::usage_error("Support to change --output not yet "
                                   "implemented");
    }

    optional< int64_t > action_id;
    if (cmdline.has_option("action"))
        action_id = cmdline.get_option< cmdline::int_option >("action");

    console_hooks hooks(ui, cmdline.has_option("show-context"));
    scan_action::drive(store_path(cmdline), action_id, hooks);
    hooks.print_tests();

    return EXIT_SUCCESS;
}
