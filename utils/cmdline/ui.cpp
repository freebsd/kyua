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

#include "utils/cmdline/ui.hpp"

#include <iostream>

#include "utils/cmdline/globals.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/sanity.hpp"

namespace cmdline = utils::cmdline;


/// Destructor for the class.
cmdline::ui::~ui(void)
{
}


/// Writes a line to stderr.
///
/// \param message The line to print, without the trailing newline character.
void
cmdline::ui::err(const std::string& message)
{
    PRE(message.empty() || message[message.length() - 1] != '\n');
    LI(F("stderr: %s") % message);
    std::cerr << message << "\n";
}


/// Writes a line to stdout.
///
/// \param message The line to print, without the trailing newline character.
void
cmdline::ui::out(const std::string& message)
{
    PRE(message.empty() || message[message.length() - 1] != '\n');
    LI(F("stdout: %s") % message);
    std::cout << message << "\n";
}


/// Formats and prints an error message.
///
/// \param ui_ The user interface object used to print the message.
/// \param message The message to print.  Must not end with a dot nor with a
///     newline character.
void
cmdline::print_error(ui* ui_, const std::string& message)
{
    PRE(!message.empty() && message[message.length() - 1] != '.');
    LE(message);
    ui_->err(F("%s: E: %s.") % cmdline::progname() % message);
}


/// Formats and prints an informational message.
///
/// \param ui_ The user interface object used to print the message.
/// \param message The message to print.  Must not end with a dot nor with a
///     newline character.
void
cmdline::print_info(ui* ui_, const std::string& message)
{
    PRE(!message.empty() && message[message.length() - 1] != '.');
    LI(message);
    ui_->err(F("%s: I: %s.") % cmdline::progname() % message);
}


/// Formats and prints a warning message.
///
/// \param ui_ The user interface object used to print the message.
/// \param message The message to print.  Must not end with a dot nor with a
///     newline character.
void
cmdline::print_warning(ui* ui_, const std::string& message)
{
    PRE(!message.empty() && message[message.length() - 1] != '.');
    LW(message);
    ui_->err(F("%s: W: %s.") % cmdline::progname() % message);
}
