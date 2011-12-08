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
#include <fstream>
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
    /// Indirection to print the output to the correct file stream.
    cli::file_writer _writer;

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
    print_context(const engine::context& context)
    {
        _writer("===> Execution context");

        _writer(F("Current directory: %s") % context.cwd());
        const std::map< std::string, std::string >& env = context.env();
        if (env.empty())
            _writer("No environment variables recorded");
        else {
            _writer("Environment variables:");
            for (std::map< std::string, std::string >::const_iterator
                     iter = env.begin(); iter != env.end(); iter++) {
                _writer(F("    %s=%s") % (*iter).first % (*iter).second);
            }
        }
    }

    /// Prints a set of results.
    std::size_t
    print_results(const engine::test_result::result_type type,
                  const char* title)
    {
        const std::map< engine::test_result::result_type,
                        std::vector< result_data > >::const_iterator iter2 =
            _results.find(type);
        if (iter2 == _results.end())
            return 0;
        const std::vector< result_data >& all = (*iter2).second;

        _writer(F("===> %s") % title);
        for (std::vector< result_data >::const_iterator iter = all.begin();
             iter != all.end(); iter++) {
            _writer(F("%s:%s  ->  %s") % (*iter).binary_path %
                    (*iter).test_case_name %
                    cli::format_result((*iter).result));
        }
        return all.size();
    }

public:
    /// Constructor for the hooks.
    ///
    /// \param ui_ The user interface object of the caller command.
    /// \param outfile_ The file to which to send the output.
    /// \param show_context_ Whether to include the runtime context in
    ///     the output or not.
    console_hooks(cmdline::ui* ui_, const fs::path& outfile_,
                  bool show_context_) :
        _writer(ui_, outfile_),
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
    print_tests(void)
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

        _writer("===> Summary");
        _writer(F("Action: %d") % _action_id);
        _writer(F("Test cases: %d total, %d skipped, %d expected failures, "
                  "%d broken, %d failed") %
                _total % skipped % xfail % broken % failed);
    }
};


}  // anonymous namespace


/// Constructs an output selector option for the report command.
cli::output_option::output_option(void) :
    cmdline::base_option('o', "output",
                         "The format of the output and the location for "
                         "the output",
                         "format:output", "console:/dev/stdout")
{
}


/// Destructor.
cli::output_option::~output_option(void)
{
}


/// Converts a user string to a format identifier.
///
/// \param value The user string representing a format name.
///
/// \return The format identifier.
///
/// \throw std::runtime_error If the input string is invalid.
cli::output_option::format_type
cli::output_option::format_from_string(const std::string& value)
{
    if (value == "console")
        return console_format;
    else
        throw std::runtime_error(F("Unknown output format '%s'") % value);
}


/// Splits an output selector into its output format and its location.
///
/// \param raw_value The argument representing an output selector as provided by
///     the user.
///
/// \return The input value split in its format type and location.
///
/// \throw std::runtime_error If the argument has an invalid syntax.
/// \throw fs::error If the location provided in the argument is invalid.
cli::output_option::option_type
cli::output_option::split_value(const std::string& raw_value)
{
    const std::string::size_type pos = raw_value.find(':');
    if (pos == std::string::npos)
        throw std::runtime_error("Argument must be of the form "
                                 "format:path");
    return std::make_pair(format_from_string(raw_value.substr(0, pos)),
                          fs::path(raw_value.substr(pos + 1)));
}


/// Ensures that an output selector argument passed to the option is valid.
///
/// \param raw_value The argument representing an output selector as provided by
///     the user.
///
/// \throw cmdline::option_argument_value_error If the output selector provided
///     in raw_value is invalid.
void
cli::output_option::validate(const std::string& raw_value) const
{
    try {
        (void)split_value(raw_value);
    } catch (const std::runtime_error& e) {
        throw cmdline::option_argument_value_error(
            F("--%s") % long_name(), raw_value, e.what());
    }
}


/// Splits an output selector argument into an output format and a location.
///
/// \param raw_value The argument representing an output selector as provided by
///     the user.
///
/// \return The output format and the location.
///
/// \pre validate(raw_value) must be true.
cli::output_option::option_type
cli::output_option::convert(const std::string& raw_value)
{
    try {
        return split_value(raw_value);
    } catch (const std::runtime_error& e) {
        UNREACHABLE;
    }
}


const fs::path cli::file_writer::_stdout_path("/dev/stdout");
const fs::path cli::file_writer::_stderr_path("/dev/stderr");


/// Constructs a new file_writer wrapper.
///
/// \param ui_ The UI object of the caller command.
/// \param path_ The path to the output file.
cli::file_writer::file_writer(cmdline::ui* const ui_, const fs::path& path_) :
    _ui(ui_), _output_path(path_)
{
    if (path_ != _stdout_path && path_ != _stderr_path) {
        _output_file.reset(new std::ofstream(path_.c_str()));
        if (!*(_output_file)) {
            throw std::runtime_error(F("Cannot open output file %s") % path_);
        }
    }
}

/// Destructor.
cli::file_writer::~file_writer(void)
{
}

/// Writes a message to the selected output.
///
/// \param message The message to write; should not include a termination
///     new line.
void
cli::file_writer::operator()(const std::string& message)
{
    if (_output_path == _stdout_path)
        _ui->out(message);
    else if (_output_path == _stderr_path)
        _ui->err(message);
    else {
        INV(_output_file.get() != NULL);
        (*_output_file) << message << '\n';
    }
}


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
    add_option(output_option());
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
    const output_option::option_type output =
        cmdline.get_option< output_option >("output");

    optional< int64_t > action_id;
    if (cmdline.has_option("action"))
        action_id = cmdline.get_option< cmdline::int_option >("action");

    INV(output.first == output_option::console_format);
    console_hooks hooks(ui, output.second, cmdline.has_option("show-context"));
    scan_action::drive(store_path(cmdline), action_id, hooks);
    hooks.print_tests();

    return EXIT_SUCCESS;
}
