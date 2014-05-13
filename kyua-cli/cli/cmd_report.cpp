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

#include "cli/cmd_report.hpp"

#include <cstddef>
#include <cstdlib>
#include <map>
#include <vector>

#include "cli/common.ipp"
#include "cli/report_console.hpp"
#include "engine/action.hpp"
#include "engine/context.hpp"
#include "engine/drivers/scan_action.hpp"
#include "engine/test_result.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace scan_action = engine::drivers::scan_action;

using cli::cmd_report;
using utils::optional;


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
    add_option(results_filter_option);
}


/// Entry point for the "report" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
/// \param unused_user_config The runtime configuration of the program.
///
/// \return 0 if everything is OK, 1 if the statement is invalid or if there is
/// any other problem.
int
cmd_report::run(cmdline::ui* ui, const cmdline::parsed_cmdline& cmdline,
                const config::tree& UTILS_UNUSED_PARAM(user_config))
{
    const output_option::option_type output =
        cmdline.get_option< output_option >("output");

    optional< int64_t > action_id;
    if (cmdline.has_option("action"))
        action_id = cmdline.get_option< cmdline::int_option >("action");

    const result_types types = get_result_types(cmdline);
    INV(output.first == output_option::console_format);
    report_console_hooks hooks(ui, output.second,
                               cmdline.has_option("show-context"), types);
    scan_action::drive(store_path(cmdline), action_id, hooks);

    return EXIT_SUCCESS;
}
