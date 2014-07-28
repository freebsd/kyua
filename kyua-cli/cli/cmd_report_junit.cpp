// Copyright 2014 Google Inc.
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

#include "cli/cmd_report_junit.hpp"

#include <cstddef>
#include <cstdlib>

#include "cli/common.ipp"
#include "engine/drivers/scan_results.hpp"
#include "engine/report_junit.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/defs.hpp"
#include "utils/optional.ipp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;
namespace fs = utils::fs;
namespace scan_results = engine::drivers::scan_results;

using cli::cmd_report_junit;
using utils::optional;


/// Default constructor for cmd_report.
cmd_report_junit::cmd_report_junit(void) : cli_command(
    "report-junit", "", 0, 0,
    "Generates a JUnit report with the result of a previous action")
{
    add_option(results_file_option);
    add_option(cmdline::path_option("output", "Path to the output file", "path",
                                    "/dev/stdout"));
}


/// Entry point for the "report" subcommand.
///
/// \param unused_ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
/// \param unused_user_config The runtime configuration of the program.
///
/// \return 0 if everything is OK, 1 if the statement is invalid or if there is
/// any other problem.
int
cmd_report_junit::run(cmdline::ui* UTILS_UNUSED_PARAM(ui),
                      const cmdline::parsed_cmdline& cmdline,
                      const config::tree& UTILS_UNUSED_PARAM(user_config))
{
    std::auto_ptr< std::ostream > output = open_output_file(
        cmdline.get_option< cmdline::path_option >("output"));

    engine::report_junit_hooks hooks(*output.get());
    scan_results::drive(results_file_open(cmdline), hooks);

    return EXIT_SUCCESS;
}
