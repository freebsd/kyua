// Copyright 2010, Google Inc.
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

/// \file utils/cmdline/base_command.hpp
/// Provides the utils::cmdline::base_command class.

#if !defined(UTILS_CMDLINE_BASE_COMMAND_HPP)
#define UTILS_CMDLINE_BASE_COMMAND_HPP

#include <string>

#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/noncopyable.hpp"

namespace utils {
namespace cmdline {


class ui;


/// Base class for the implementation of subcommands of a program.
///
/// The main CLI binary subclasses this class to define the subcommands it
/// provides.  Each subcommand has a name, a set of options and a specific
/// syntax for the arguments it receives.  The subclass also implements the
/// entry point for the code of the command.
class base_command : noncopyable {
    const std::string _name;
    const std::string _arg_list;
    int _min_args;
    int _max_args;
    const std::string _short_description;
    options_vector _options;

    void add_option_ptr(const base_option*);

protected:
    template< typename Option > void add_option(const Option&);

    /// Main code of the command.
    ///
    /// This is called from main() after the command line has been processed and
    /// validated.
    ///
    /// \param ui Object to interact with the I/O of the command.  The command
    ///     must always use this object to write to stdout and stderr.
    /// \param cmdline The parsed command line, containing the values of any
    ///     given options and arguments.
    ///
    /// \return The exit code that the program has to return.  0 on success,
    ///     some other value on error.
    ///
    /// \throw std::runtime_error Any errors detected during the execution of
    ///     the command are reported by means of exceptions.
    virtual int run(ui* ui, const parsed_cmdline& cmdline) = 0;

public:
    explicit base_command(const std::string&, const std::string&,
                          const int, const int, const std::string&);
    virtual ~base_command(void);

    const std::string& name(void) const;
    const std::string& arg_list(void) const;
    const std::string& short_description(void) const;
    const options_vector& options(void) const;

    int main(ui*, const args_vector&);
};


}  // namespace cmdline
}  // namespace utils


#endif  // !defined(UTILS_CMDLINE_BASE_COMMAND_HPP)
