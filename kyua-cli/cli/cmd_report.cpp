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
#include <ostream>
#include <string>
#include <vector>

#include "cli/common.ipp"
#include "engine/context.hpp"
#include "engine/drivers/scan_results.hpp"
#include "engine/test_result.hpp"
#include "store/read_transaction.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui.hpp"
#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace scan_results = engine::drivers::scan_results;

using cli::cmd_report;
using utils::optional;


namespace {


/// Generates a plain-text report intended to be printed to the console.
class report_console_hooks : public engine::drivers::scan_results::base_hooks {
    /// Stream to which to write the report.
    std::ostream& _output;

    /// Whether to include the runtime context in the output or not.
    const bool _show_context;

    /// Collection of result types to include in the report.
    const cli::result_types& _results_filters;

    /// Path to the store file being read.
    const fs::path& _store_file;

    /// The total run time of the tests.
    utils::datetime::delta _runtime;

    /// Representation of a single result.
    struct result_data {
        /// The relative path to the test program.
        utils::fs::path binary_path;

        /// The name of the test case.
        std::string test_case_name;

        /// The result of the test case.
        engine::test_result result;

        /// The duration of the test case execution.
        utils::datetime::delta duration;

        /// Constructs a new results data.
        ///
        /// \param binary_path_ The relative path to the test program.
        /// \param test_case_name_ The name of the test case.
        /// \param result_ The result of the test case.
        /// \param duration_ The duration of the test case execution.
        result_data(const utils::fs::path& binary_path_,
                    const std::string& test_case_name_,
                    const engine::test_result& result_,
                    const utils::datetime::delta& duration_) :
            binary_path(binary_path_), test_case_name(test_case_name_),
            result(result_), duration(duration_)
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
        _output << "===> Execution context\n";

        _output << F("Current directory: %s\n") % context.cwd();
        const std::map< std::string, std::string >& env = context.env();
        if (env.empty())
            _output << "No environment variables recorded\n";
        else {
            _output << "Environment variables:\n";
            for (std::map< std::string, std::string >::const_iterator
                     iter = env.begin(); iter != env.end(); iter++) {
                _output << F("    %s=%s\n") % (*iter).first % (*iter).second;
            }
        }
    }

    /// Counts how many results of a given type have been received.
    std::size_t
    count_results(const engine::test_result::result_type type)
    {
        const std::map< engine::test_result::result_type,
                        std::vector< result_data > >::const_iterator iter =
            _results.find(type);
        if (iter == _results.end())
            return 0;
        else
            return (*iter).second.size();
    }

    /// Prints a set of results.
    void
    print_results(const engine::test_result::result_type type,
                  const char* title)
    {
        const std::map< engine::test_result::result_type,
                        std::vector< result_data > >::const_iterator iter2 =
            _results.find(type);
        if (iter2 == _results.end())
            return;
        const std::vector< result_data >& all = (*iter2).second;

        _output << F("===> %s\n") % title;
        for (std::vector< result_data >::const_iterator iter = all.begin();
             iter != all.end(); iter++) {
            _output << F("%s:%s  ->  %s  [%s]\n") % (*iter).binary_path %
                (*iter).test_case_name %
                cli::format_result((*iter).result) %
                cli::format_delta((*iter).duration);
        }
    }

public:
    /// Constructor for the hooks.
    ///
    /// \param [out] output_ Stream to which to write the report.
    /// \param show_context_ Whether to include the runtime context in
    ///     the output or not.
    /// \param results_filters_ The result types to include in the report.
    ///     Cannot be empty.
    /// \param store_file_ Path to the store file being read.
    report_console_hooks(std::ostream& output_, const bool show_context_,
                         const cli::result_types& results_filters_,
                         const fs::path& store_file_) :
        _output(output_),
        _show_context(show_context_),
        _results_filters(results_filters_),
        _store_file(store_file_)
    {
        PRE(!results_filters_.empty());
    }

    /// Callback executed when the context is loaded.
    ///
    /// \param context The context loaded from the database.
    void
    got_context(const engine::context& context)
    {
        if (_show_context)
            print_context(context);
    }

    /// Callback executed when a test results is found.
    ///
    /// \param iter Container for the test result's data.
    void
    got_result(store::results_iterator& iter)
    {
        _runtime += iter.duration();
        const engine::test_result result = iter.result();
        _results[result.type()].push_back(
            result_data(iter.test_program()->relative_path(),
                        iter.test_case_name(), iter.result(), iter.duration()));
    }

    /// Prints the tests summary.
    ///
    /// \param unused_r Result of the scan_results driver execution.
    void
    end(const engine::drivers::scan_results::result& UTILS_UNUSED_PARAM(r))
    {
        using engine::test_result;
        typedef std::map< test_result::result_type, const char* > types_map;

        types_map titles;
        titles[engine::test_result::broken] = "Broken tests";
        titles[engine::test_result::expected_failure] = "Expected failures";
        titles[engine::test_result::failed] = "Failed tests";
        titles[engine::test_result::passed] = "Passed tests";
        titles[engine::test_result::skipped] = "Skipped tests";

        for (cli::result_types::const_iterator iter = _results_filters.begin();
             iter != _results_filters.end(); ++iter) {
            const types_map::const_iterator match = titles.find(*iter);
            INV_MSG(match != titles.end(), "Conditional does not match user "
                    "input validation in parse_types()");
            print_results((*match).first, (*match).second);
        }

        const std::size_t broken = count_results(test_result::broken);
        const std::size_t failed = count_results(test_result::failed);
        const std::size_t passed = count_results(test_result::passed);
        const std::size_t skipped = count_results(test_result::skipped);
        const std::size_t xfail = count_results(test_result::expected_failure);
        const std::size_t total = broken + failed + passed + skipped + xfail;

        _output << "===> Summary\n";
        _output << F("Results read from %s\n") % _store_file;
        _output << F("Test cases: %s total, %s skipped, %s expected failures, "
                     "%s broken, %s failed\n") %
            total % skipped % xfail % broken % failed;
        _output << F("Total time: %s\n") % cli::format_delta(_runtime);
    }
};


}  // anonymous namespace


/// Default constructor for cmd_report.
cmd_report::cmd_report(void) : cli_command(
    "report", "", 0, 0,
    "Generates a report with the result of a previous action")
{
    add_option(results_file_option);
    add_option(cmdline::bool_option(
        "show-context", "Include the execution context in the report"));
    add_option(cmdline::path_option("output", "Path to the output file", "path",
                                    "/dev/stdout"));
    add_option(results_filter_option);
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
cmd_report::run(cmdline::ui* UTILS_UNUSED_PARAM(ui),
                const cmdline::parsed_cmdline& cmdline,
                const config::tree& UTILS_UNUSED_PARAM(user_config))
{
    std::auto_ptr< std::ostream > output = open_output_file(
        cmdline.get_option< cmdline::path_option >("output"));

    const fs::path store_file = results_file_open(cmdline);

    const result_types types = get_result_types(cmdline);
    report_console_hooks hooks(*output.get(),
                               cmdline.has_option("show-context"), types,
                               store_file);
    scan_results::drive(store_file, hooks);

    return EXIT_SUCCESS;
}
