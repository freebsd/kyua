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

#include "engine/atf_iface/test_program.hpp"

#include <cstdlib>

#include "engine/atf_iface/test_case.hpp"
#include "engine/exceptions.hpp"
#include "engine/test_result.hpp"
#include "utils/config/exceptions.hpp"
#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"
#include "utils/process/children.ipp"
#include "utils/process/exceptions.hpp"
#include "utils/sanity.hpp"

namespace atf_iface = engine::atf_iface;
namespace config = utils::config;
namespace process = utils::process;


namespace {


/// Internal error code used to communicate an exec failure.
static const int list_failure_exitcode = 120;


/// Splits a property line of the form "name: word1 [... wordN]".
///
/// \param line The line to parse.
///
/// \return A (property_name, property_value) pair.
///
/// \throw format_error If the value of line is invalid.
static std::pair< std::string, std::string >
split_prop_line(const std::string& line)
{
    const std::string::size_type pos = line.find(": ");
    if (pos == std::string::npos)
        throw engine::format_error("Invalid property line; expecting line of "
                                   "the form 'name: value'");
    return std::make_pair(line.substr(0, pos), line.substr(pos + 2));
}


/// Parses a set of consecutive property lines.
///
/// Processing stops when an empty line or the end of file is reached.  None of
/// these conditions indicate errors.
///
/// \param input The stream to read the lines from.
///
/// \return The parsed property lines.
///
/// throw format_error If the input stream has an invalid format.
static engine::properties_map
parse_properties(std::istream& input)
{
    engine::properties_map properties;

    std::string line;
    while (std::getline(input, line).good() && !line.empty()) {
        const std::pair< std::string, std::string > property = split_prop_line(
            line);
        if (properties.find(property.first) != properties.end())
            throw engine::format_error("Duplicate value for property " +
                                       property.first);
        properties.insert(property);
    }

    return properties;
}


/// Subprocess functor to invoke "test-program -l" to list test cases.
class list_test_cases {
    /// Absolute path to the test program to list the test cases of.
    const utils::fs::path& _program;

public:
    /// Initializes the functor.
    ///
    /// \param program The absolute path of the test program to execute.
    list_test_cases(const utils::fs::path& program) :
        _program(program)
    {
        PRE(_program.is_absolute());
    }

    /// Child process entry point.
    ///
    /// \post The process terminates.
    void
    operator()(void)
    {
        std::vector< std::string > args;
        args.push_back("-l");
        try {
            process::exec(_program, args);
        } catch (const process::error& e) {
            std::exit(list_failure_exitcode);
        }
    }
};


/// Auxiliary function for load_test_cases.
///
/// This function differs from load_test_cases in that it can throw exceptions.
/// The caller takes this into account and generates a fake test case to
/// represent the failure.
///
/// \param test_program Test program from which to load the test cases.
///
/// \return A collection of test_case objects representing the input test case
/// list.
///
/// \throw engine::error If there is a problem executing the test program.
/// \throw format_error If the test case list has an invalid format.
/// \throw process::error If the spawning of the child process fails.
static engine::test_cases_vector
safe_load_test_cases(const engine::base_test_program* test_program)
{
    LI(F("Obtaining test cases list from test program '%s' of root '%s'") %
       test_program->relative_path() % test_program->root());

    const std::auto_ptr< process::child_with_output > child =
        process::child_with_output::fork(list_test_cases(
                                             test_program->absolute_path()));

    engine::test_cases_vector loaded_test_cases;
    std::string format_error_message;
    try {
        loaded_test_cases = atf_iface::detail::parse_test_cases(
            *test_program, child->output());
    } catch (const engine::format_error& e) {
        format_error_message = F("%s: %s") % test_program->relative_path().str()
            % e.what();
    }

    const utils::process::status status = child->wait();
    if (status.exited() && status.exitstatus() == list_failure_exitcode)
        throw engine::error("Failed to execute the test program");
    if (!status.exited() || status.exitstatus() != EXIT_SUCCESS)
        throw engine::error("Test program did not exit cleanly");
    if (!format_error_message.empty())
        throw engine::format_error(format_error_message);

    return loaded_test_cases;
}


}  // anonymous namespace


/// Parses the metadata of an ATF test case.
///
/// \param raw_properties The properties (name/value string pairs) as provided
///     by the ATF test program.
///
/// \return A parsed metadata object.
///
/// \throw engine::format_error If the syntax of any of the properties is
///     invalid.
engine::metadata
engine::atf_iface::detail::parse_metadata(const properties_map& raw_properties)
{
    metadata_builder mdbuilder;

    try {
        for (properties_map::const_iterator iter = raw_properties.begin();
             iter != raw_properties.end(); iter++) {
            const std::string& name = (*iter).first;
            const std::string& value = (*iter).second;

            if (name == "descr") {
                mdbuilder.set_string("description", value);
            } else if (name == "has.cleanup") {
                mdbuilder.set_string("has_cleanup", value);
            } else if (name == "require.arch") {
                mdbuilder.set_string("allowed_architectures", value);
            } else if (name == "require.config") {
                mdbuilder.set_string("required_configs", value);
            } else if (name == "require.files") {
                mdbuilder.set_string("required_files", value);
            } else if (name == "require.machine") {
                mdbuilder.set_string("allowed_platforms", value);
            } else if (name == "require.memory") {
                mdbuilder.set_string("required_memory", value);
            } else if (name == "require.progs") {
                mdbuilder.set_string("required_programs", value);
            } else if (name == "require.user") {
                mdbuilder.set_string("required_user", value);
            } else if (name == "timeout") {
                mdbuilder.set_string("timeout", value);
            } else if (name.length() > 2 && name.substr(0, 2) == "X-") {
                mdbuilder.add_custom(name, value);
            } else {
                throw engine::format_error(F("Unknown test case metadata "
                                             "property '%s'") % name);
            }
        }
    } catch (const config::error& e) {
        throw engine::format_error(e.what());
    }

    return mdbuilder.build();
}


/// Parses the list of test cases generated by a test program.
///
/// This method is exposed in the detail namespace to allow unit testing of the
/// parser without having to rely on a binary that generates the list.
///
/// \param program The name of the test program binary from which the test case
///     list is being extracted.
/// \param input An input stream that yields the list of test cases.
///
/// \return A collection of test_case objects representing the input test case
/// list.
///
/// \throw format_error If the test case list has an invalid format.
engine::test_cases_vector
engine::atf_iface::detail::parse_test_cases(const base_test_program& program,
                                            std::istream& input)
{
    std::string line;

    std::getline(input, line);
    if (line != "Content-Type: application/X-atf-tp; version=\"1\""
        || !input.good())
        throw format_error(F("Invalid header for test case list; expecting "
                             "Content-Type for application/X-atf-tp version 1, "
                             "got '%s'") % line);

    std::getline(input, line);
    if (!line.empty() || !input.good())
        throw format_error(F("Invalid header for test case list; expecting "
                             "a blank line, got '%s'") % line);

    test_cases_vector test_cases;
    while (std::getline(input, line).good()) {
        const std::pair< std::string, std::string > ident = split_prop_line(
            line);
        if (ident.first != "ident" or ident.second.empty())
            throw format_error("Invalid test case definition; must be "
                               "preceeded by the identifier");

        const properties_map raw_properties = parse_properties(input);
        const engine::metadata md = detail::parse_metadata(raw_properties);
        const atf_iface::test_case test_case =
            atf_iface::test_case(program, ident.second, md);
        test_cases.push_back(engine::test_case_ptr(
             new atf_iface::test_case(test_case)));
    }
    if (test_cases.empty())
        throw format_error("No test cases");
    return test_cases;
}


/// Constructs a new ATF test program.
///
/// \param binary_ The name of the test program binary relative to root_.
/// \param root_ The root of the test suite containing the test program.
/// \param test_suite_name_ The name of the test suite this program belongs to.
///
atf_iface::test_program::test_program(const utils::fs::path& binary_,
                                      const utils::fs::path& root_,
                                      const std::string& test_suite_name_) :
    base_test_program("atf", binary_, root_, test_suite_name_)
{
}


/// Loads the list of test cases contained in a test program.
///
/// \param test_program Test program from which to load the test cases.
///
/// \return A collection of test_case objects representing the input test case
/// list.  If the test cases cannot be properly loaded from the test program,
/// the return list contains a single test case representing the failure.  The
/// fake test case return is "runnable" in the sense that it will report an
/// error when attempted to be run.
engine::test_cases_vector
atf_iface::load_atf_test_cases(const base_test_program* test_program)
{
    try {
        return safe_load_test_cases(test_program);
    } catch (const std::runtime_error& e) {
        test_cases_vector loaded_test_cases;
        loaded_test_cases.push_back(test_case_ptr(new test_case(
            *test_program, "__test_cases_list__",
            "Represents the correct processing of the test cases list",
            test_result(test_result::broken, e.what()))));
        return loaded_test_cases;
    }
}
