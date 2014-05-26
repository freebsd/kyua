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

/// \file cli/cmd_report.hpp
/// Provides the cmd_report class.

#if !defined(CLI_CMD_REPORT_HPP)
#define CLI_CMD_REPORT_HPP

#include <fstream>
#include <memory>

#include "cli/common.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/fs/path.hpp"
#include "utils/noncopyable.hpp"

namespace cli {


/// Option to specify an output selector.
///
/// An output selector is composed of an output format and a location for the
/// output.  The output format is something like "html" whereas the location is
/// either a file or a directory on disk.  The semantics of the location vary
/// depending on the format.
class output_option : public utils::cmdline::base_option {
public:
    /// Identifiers for the valid format types.
    enum format_type {
        console_format,
        junit_format,
    };

    /// Output format and location pair; i.e. the type of the native value.
    typedef std::pair< format_type, utils::fs::path > option_type;

private:
    static format_type format_from_string(const std::string&);
    static option_type split_value(const std::string&);

public:
    output_option(void);
    virtual ~output_option(void);

    virtual void validate(const std::string&) const;

    static option_type convert(const std::string&);
};


/// Implementation of the "report" subcommand.
class cmd_report : public cli_command
{
public:
    cmd_report(void);

    int run(utils::cmdline::ui*, const utils::cmdline::parsed_cmdline&,
            const utils::config::tree&);
};


}  // namespace cli


#endif  // !defined(CLI_CMD_REPORT_HPP)
