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

#include "utils/cmdline/base_command.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/sanity.hpp"

namespace cmdline = utils::cmdline;


/// Creates a new command.
///
/// \param name_ The name of the command.  Must be unique within the context of
///     a program and have no spaces.
/// \param arg_list_ A textual description of the arguments received by the
///     command.  May be empty.
/// \param min_args_ The minimum number of arguments required by the command.
/// \param max_args_ The maximum number of arguments required by the command.
///     -1 means infinity.
/// \param short_description_ A description of the purpose of the command.
cmdline::base_command::base_command(const std::string& name_,
                                    const std::string& arg_list_,
                                    const int min_args_,
                                    const int max_args_,
                                    const std::string& short_description_) :
    _name(name_),
    _arg_list(arg_list_),
    _min_args(min_args_),
    _max_args(max_args_),
    _short_description(short_description_)
{
    PRE(name_.find(' ') == std::string::npos);
    PRE(max_args_ == -1 || min_args_ <= max_args_);
}


/// Destructor for a command.
cmdline::base_command::~base_command(void)
{
    for (options_vector::const_iterator iter = _options.begin();
         iter != _options.end(); iter++)
        delete *iter;
}


/// Internal method to register a dynamically-allocated option.
///
/// Always use add_option() from subclasses to add options.
///
/// \param option_ The option to add.  Must have been dynamically allocated.
///     This grabs ownership of the pointer, which is released when the command
///     is destroyed.
void
cmdline::base_command::add_option_ptr(const cmdline::base_option* option_)
{
    try {
        _options.push_back(option_);
    } catch (...) {
        delete option_;
        throw;
    }
}


/// Gets the name of the command.
///
/// \return The command name.
const std::string&
cmdline::base_command::name(void) const
{
    return _name;
}


/// Gets the textual representation of the arguments list.
///
/// \return The description of the arguments list.
const std::string&
cmdline::base_command::arg_list(void) const
{
    return _arg_list;
}


/// Gets the description of the purpose of the command.
///
/// \return The description of the command.
const std::string&
cmdline::base_command::short_description(void) const
{
    return _short_description;
}


/// Gets the definition of the options accepted by the command.
///
/// \return The list of options.
const cmdline::options_vector&
cmdline::base_command::options(void) const
{
    return _options;
}


/// Entry point for the command.
///
/// This delegates execution to the run() abstract function after the command
/// line provided in args has been parsed.
///
/// If this function returns, the command is assumed to have been executed
/// successfully.  Any error must be reported by means of exceptions.
///
/// \param ui Object to interact with the I/O of the command.  The command must
///     always use this object to write to stdout and stderr.
/// \param args The command line passed to the command broken by word, which
///     includes options and arguments.
///
/// \return The exit code that the program has to return.  0 on success, some
///     other value on error.
/// \throw usage_error If args is invalid (i.e. if the options are mispecified
///     or if the arguments are invalid).
int
cmdline::base_command::main(cmdline::ui* ui, const cmdline::args_vector& args)
{
    PRE(name() == args[0]);
    const cmdline::parsed_cmdline cmdline = cmdline::parse(args, options());

    const int argc = cmdline.arguments().size();
    if (argc < _min_args)
        throw usage_error("Not enough arguments");
    if (_max_args != -1 && argc > _max_args)
        throw usage_error("Too many arguments");

    return run(ui, cmdline);
}
