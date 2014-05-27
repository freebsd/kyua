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

/// \file cli/report_console.hpp
/// Provides the 'console' format of the report command.

#if !defined(CLI_REPORT_CONSOLE_HPP)
#define CLI_REPORT_CONSOLE_HPP

#include <cstddef>
#include <ostream>
#include <string>

#include "cli/common.hpp"
#include "engine/drivers/scan_action.hpp"
#include "engine/test_result.hpp"
#include "utils/cmdline/ui.hpp"
#include "utils/datetime.hpp"
#include "utils/fs/path.hpp"

namespace cli {


/// Generates a plain-text report intended to be printed to the console.
class report_console_hooks : public engine::drivers::scan_action::base_hooks {
    /// Stream to which to write the report.
    std::ostream& _output;

    /// Whether to include the runtime context in the output or not.
    const bool _show_context;

    /// Collection of result types to include in the report.
    const cli::result_types& _results_filters;

    /// The action ID loaded.
    int64_t _action_id;

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

    void print_context(const engine::context&);

    std::size_t count_results(const engine::test_result::result_type);

    void print_results(const engine::test_result::result_type, const char*);

public:
    report_console_hooks(std::ostream&, const bool, const cli::result_types&);

    void got_action(const int64_t, const engine::action&);
    void got_result(store::results_iterator&);

    void end(const engine::drivers::scan_action::result&);
};


}  // namespace cli


#endif  // !defined(CLI_REPORT_CONSOLE_HPP)
