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

/// \file cli/report_junit.hpp
/// Provides the 'junit' format of the report command.

#if !defined(CLI_REPORT_JUNIT_HPP)
#define CLI_REPORT_JUNIT_HPP

#include <cstddef>

#include "cli/common.hpp"
#include "engine/drivers/scan_action.hpp"
#include "utils/cmdline/ui.hpp"
#include "utils/fs/path.hpp"

namespace cli {


/// Generates a plain-text report intended to be printed to the junit.
class report_junit_hooks : public engine::drivers::scan_action::base_hooks {
    /// Indirection to print the output to the correct file stream.
    cli::file_writer _writer;

    /// Whether to include the runtime context in the output or not.
    const bool _show_context;

    /// Collection of result types to include in the report.
    const cli::result_types& _results_filters;

    /// The action ID loaded.
    int64_t _action_id;

public:
    report_junit_hooks(utils::cmdline::ui*, const utils::fs::path&,
                       const bool, const cli::result_types&);

    void begin(void);

    void got_action(const int64_t, const engine::action&);
    void got_result(store::results_iterator&);

    void end(const engine::drivers::scan_action::result&);
};


}  // namespace cli


#endif  // !defined(CLI_REPORT_JUNIT_HPP)
