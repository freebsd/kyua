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

#include "engine/report_junit.hpp"

#include <algorithm>

#include "engine/context.hpp"
#include "engine/metadata.hpp"
#include "engine/test_result.hpp"
#include "store/read_transaction.hpp"
#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/text/operations.hpp"

namespace config = utils::config;
namespace datetime = utils::datetime;
namespace scan_results = engine::drivers::scan_results;
namespace text = utils::text;


/// Converts a test program name into a class-like name.
///
/// \param test_program Test program from which to extract the name.
///
/// \return A class-like representation of the test program's identifier.
std::string
engine::junit_classname(const engine::test_program& test_program)
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
std::string
engine::junit_duration(const datetime::delta& delta)
{
    return F("%.3s") % (delta.seconds + (delta.useconds / 1000000.0));
}


/// String to prepend to the formatted test case metadata.
const char* const engine::junit_metadata_prefix =
    "Test case metadata\n"
    "------------------\n"
    "\n";


/// String to append to the formatted test case metadata.
const char* const engine::junit_metadata_suffix =
    "\n"
    "Original stderr\n"
    "---------------\n"
    "\n";


/// Formats a test's metadata for recording in stderr.
///
/// \param metadata The metadata to format.
///
/// \return A string with the metadata contents that can be prefixed to the
/// original test's stderr.
std::string
engine::junit_metadata(const engine::metadata& metadata)
{
    const engine::properties_map props = metadata.to_properties();
    if (props.empty())
        return "";

    std::ostringstream output;
    output << junit_metadata_prefix;
    for (engine::properties_map::const_iterator iter = props.begin();
         iter != props.end(); ++iter) {
        if ((*iter).second.empty()) {
            output << F("%s is empty\n") % (*iter).first;
        } else {
            output << F("%s = %s\n") % (*iter).first % (*iter).second;
        }
    }
    output << junit_metadata_suffix;
    return output.str();
}


/// Constructor for the hooks.
///
/// \param [out] output_ Stream to which to write the report.
engine::report_junit_hooks::report_junit_hooks(std::ostream& output_) :
    _output(output_)
{
}


/// Callback executed when the context is loaded.
///
/// \param context The context loaded from the database.
void
engine::report_junit_hooks::got_context(const engine::context& context)
{
    _output << "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n";
    _output << "<testsuite>\n";

    _output << "<properties>\n";
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


/// Callback executed when a test results is found.
///
/// \param iter Container for the test result's data.
void
engine::report_junit_hooks::got_result(store::results_iterator& iter)
{
    const engine::test_result result = iter.result();

    _output << F("<testcase classname=\"%s\" name=\"%s\" time=\"%s\">\n")
        % text::escape_xml(junit_classname(*iter.test_program()))
        % text::escape_xml(iter.test_case_name())
        % junit_duration(iter.duration());

    std::string stderr_contents;

    switch (result.type()) {
    case engine::test_result::failed:
        _output << F("<failure message=\"%s\"/>\n")
            % text::escape_xml(result.reason());
        break;

    case engine::test_result::expected_failure:
        stderr_contents += ("Expected failure result details\n"
                            "-------------------------------\n"
                            "\n"
                            + result.reason() + "\n"
                            "\n");
        break;

    case engine::test_result::passed:
        // Passed results have no status nodes.
        break;

    case engine::test_result::skipped:
        _output << "<skipped/>\n";
        stderr_contents += ("Skipped result details\n"
                            "----------------------\n"
                            "\n"
                            + result.reason() + "\n"
                            "\n");
        break;

    default:
        _output << F("<error message=\"%s\"/>\n")
            % text::escape_xml(result.reason());
    }

    const std::string stdout_contents = iter.stdout_contents();
    if (!stdout_contents.empty()) {
        _output << F("<system-out>%s</system-out>\n")
            % text::escape_xml(stdout_contents);
    }

    {
        const engine::test_case_ptr test_case = iter.test_program()->find(
            iter.test_case_name());
        stderr_contents += junit_metadata(test_case->get_metadata());
    }
    {
        const std::string real_stderr_contents = iter.stderr_contents();
        if (real_stderr_contents.empty()) {
            stderr_contents += "<EMPTY>\n";
        } else {
            stderr_contents += real_stderr_contents;
        }
    }
    _output << "<system-err>" << text::escape_xml(stderr_contents)
            << "</system-err>\n";

    _output << "</testcase>\n";
}


/// Finalizes the report.
///
/// \param unused_r The result of the driver execution.
void
engine::report_junit_hooks::end(
    const scan_results::result& UTILS_UNUSED_PARAM(r))
{
    _output << "</testsuite>\n";
}
