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

extern "C" {
#include <sys/ioctl.h>

#include <unistd.h>
}

#include <iostream>

#include "utils/cmdline/globals.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/optional.ipp"
#include "utils/text/operations.ipp"
#include "utils/sanity.hpp"

namespace cmdline = utils::cmdline;

using utils::none;
using utils::optional;


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


/// Queries the width of the screen.
///
/// This information comes first from the COLUMNS environment variable.  If not
/// present or invalid, and if the stdout of the current process is connected to
/// a terminal the width is deduced from the terminal itself.  Ultimately, if
/// all fails, none is returned.  This function shall not raise any errors.
///
/// Be aware that the results of this query are cached during execution.
/// Subsequent calls to this function will always return the same value even if
/// the terminal size has actually changed.
///
/// \todo Install a signal handler for SIGWINCH so that we can readjust our
/// knowledge of the terminal width when the user resizes the window.
///
/// \return The width of the screen if it was possible to determine it, or none
/// otherwise.
optional< std::size_t >
cmdline::ui::screen_width(void) const
{
    static bool done = false;
    static optional< std::size_t > width = none;

    if (!done) {
        const optional< std::string > columns = utils::getenv("COLUMNS");
        if (columns) {
            if (columns.get().length() > 0) {
                try {
                    width = utils::make_optional(
                        utils::text::to_type< std::size_t >(columns.get()));
                } catch (const utils::text::value_error& e) {
                    LD(F("Ignoring invalid value in COLUMNS variable: %s") %
                       e.what());
                }
            }
        }
        if (!width) {
            struct ::winsize ws;
            if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1)
                width = optional< std::size_t >(ws.ws_col);
        }

        if (width && width.get() >= 80)
            width.get() -= 5;

        done = true;
    }

    return width;
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
