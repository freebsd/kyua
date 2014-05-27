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

#include "cli/report_junit.hpp"

#include <algorithm>

#include "cli/common.ipp"
#include "engine/action.hpp"
#include "engine/context.hpp"
#include "utils/datetime.hpp"
#include "utils/format/macros.hpp"
#include "utils/sanity.hpp"
#include "utils/text/operations.hpp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace scan_action = engine::drivers::scan_action;
namespace text = utils::text;


namespace {


/// Converts a test program name into a class-like name.
///
/// \param test_program Test program from which to extract the name.
///
/// \return A class-like representation of the test program's identifier.
static std::string
junit_classname(const engine::test_program& test_program)
{
    std::string classname = test_program.relative_path().str();
    std::replace(classname.begin(), classname.end(), '/', '.');
    return classname;
}


/// Converts a test case's duration to a second-based representation.
///
/// \param delta The duration to convert.
///
/// \return A second-based with millisecond-precision representation of the
/// input duration.
static std::string
junit_duration(const datetime::delta& delta)
{
    return F("%.3s") % (delta.seconds + (delta.useconds / 1000000.0));
}


}  // anonymous namespace


/// Constructor for the hooks.
///
/// \param [out] output_ Stream to which to write the report.
/// \param show_context_ Whether to include the runtime context in
///     the output or not.
/// \param results_filters_ The result types to include in the report.
///     Cannot be empty.
cli::report_junit_hooks::report_junit_hooks(
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
cli::report_junit_hooks::got_action(const int64_t action_id,
                                    const engine::action& action)
{
    _output << "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n";
    _output << "<testsuite>\n";

    _action_id = action_id;
    if (_show_context) {
        const engine::context& context = action.runtime_context();
        _output << "<properties>\n";
        _output << F("<property name=\"kyua.action_id\" value=\"%s\"/>\n")
            % _action_id;
        _output << F("<property name=\"cwd\" value=\"%s\"/>\n")
            % text::escape_xml(context.cwd().str());
        for (config::properties_map::const_iterator iter =
                 context.env().begin(); iter != context.env().end(); ++iter) {
            _output << F("<property name=\"env.%s\" value=\"%s\"/>\n")
                % text::escape_xml((*iter).first)
                % text::escape_xml((*iter).second);
        }
        _output << "</properties>\n";
    }
}


/// Callback executed when a test results is found.
///
/// \param iter Container for the test result's data.
void
cli::report_junit_hooks::got_result(store::results_iterator& iter)
{
    const engine::test_result result = iter.result();

    if (std::find(_results_filters.begin(), _results_filters.end(),
                  result.type()) == _results_filters.end())
        return;

    _output << F("<testcase classname=\"%s\" name=\"%s\" time=\"%s\">\n")
        % text::escape_xml(junit_classname(*iter.test_program()))
        % text::escape_xml(iter.test_case_name())
        % junit_duration(iter.duration());

    switch (result.type()) {
    case engine::test_result::failed:
        _output << F("<failure message=\"%s\"/>\n")
            % text::escape_xml(result.reason());
        break;

    case engine::test_result::passed:
        // Passed results have no status nodes.
        break;

    case engine::test_result::skipped:
        _output << "<skipped/>\n";
        break;

    default:
        _output << F("<error message=\"%s\"/>\n")
            % text::escape_xml(result.reason());
    }

    _output << F("<system-out>%s</system-out>\n")
            % text::escape_xml(iter.stdout_contents());
    _output << F("<system-err>%s</system-err>\n")
            % text::escape_xml(iter.stderr_contents());
    _output << "</testcase>\n";
}


/// Finalizes the report.
///
/// \param unused_r The result of the driver execution.
void
cli::report_junit_hooks::end(const scan_action::result& UTILS_UNUSED_PARAM(r))
{
    _output << "</testsuite>\n";
}
