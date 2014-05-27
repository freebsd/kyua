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

#include "cli/report_console.hpp"

#include <cstddef>
#include <map>
#include <vector>

#include "cli/common.ipp"
#include "engine/action.hpp"
#include "engine/context.hpp"
#include "utils/format/macros.hpp"
#include "utils/sanity.hpp"

namespace cmdline = utils::cmdline;
namespace fs = utils::fs;
namespace scan_action = engine::drivers::scan_action;


/// Prints the execution context to the output.
///
/// \param context The context to dump.
void
cli::report_console_hooks::print_context(const engine::context& context)
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
cli::report_console_hooks::count_results(
    const engine::test_result::result_type type)
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
cli::report_console_hooks::print_results(
    const engine::test_result::result_type type,
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


/// Constructor for the hooks.
///
/// \param [out] output_ Stream to which to write the report.
/// \param show_context_ Whether to include the runtime context in
///     the output or not.
/// \param results_filters_ The result types to include in the report.
///     Cannot be empty.
cli::report_console_hooks::report_console_hooks(
    std::ostream& output_,
    const bool show_context_,
    const cli::result_types& results_filters_) :
    _output(output_),
    _show_context(show_context_),
    _results_filters(results_filters_)
{
    PRE(!results_filters_.empty());
}


/// Callback executed when an action is found.
///
/// \param action_id The identifier of the loaded action.
/// \param action The action loaded from the database.
void
cli::report_console_hooks::got_action(const int64_t action_id,
                                      const engine::action& action)
{
    _action_id = action_id;
    if (_show_context)
        print_context(action.runtime_context());
}


/// Callback executed when a test results is found.
///
/// \param iter Container for the test result's data.
void
cli::report_console_hooks::got_result(store::results_iterator& iter)
{
    _runtime += iter.duration();
    const engine::test_result result = iter.result();
    _results[result.type()].push_back(
        result_data(iter.test_program()->relative_path(),
                    iter.test_case_name(), iter.result(), iter.duration()));
}


/// Prints the tests summary.
///
/// \param unused_r Result of the scan_action driver execution.
void
cli::report_console_hooks::end(const scan_action::result& UTILS_UNUSED_PARAM(r))
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
    _output << F("Action: %s\n") % _action_id;
    _output << F("Test cases: %s total, %s skipped, %s expected failures, "
                 "%s broken, %s failed\n") %
        total % skipped % xfail % broken % failed;
    _output << F("Total time: %s\n") % cli::format_delta(_runtime);
}
