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

#include <fcntl.h>
#include <unistd.h>
}

#include <cerrno>
#include <cstring>

#include <atf-c++.hpp>

#include "utils/cmdline/globals.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/optional.ipp"

namespace cmdline = utils::cmdline;

using utils::none;
using utils::optional;


namespace {


/// Trivial implementation of the ui interface for testing purposes.
class ui_test : public cmdline::ui {
public:
    /// Recording of the last call to err().
    std::string err_message;

    /// Recording of the last call to out().
    std::string out_message;

    /// Records an error message.
    ///
    /// \pre This function has not been called before.  We only record a single
    ///     message for simplicity of the testing.
    ///
    /// \param message The message to record.
    void
    err(const std::string& message)
    {
        ATF_REQUIRE(err_message.empty());
        err_message = message;
    }

    /// Records a message.
    ///
    /// \pre This function has not been called before.  We only record a single
    ///     message for simplicity of the testing.
    ///
    /// \param message The message to record.
    void
    out(const std::string& message)
    {
        ATF_REQUIRE(out_message.empty());
        out_message = message;
    }

    /// Queries the width of the screen.
    ///
    /// \return Always none, as we do not want to depend on line wrapping in our
    /// tests.
    optional< std::size_t >
    screen_width(void) const
    {
        return none;
    }
};


/// Reopens stdout as a tty and returns its width.
///
/// \return The width of the tty in columns.  If the width is wider than 80, the
/// result is 5 columns narrower to match the screen_width() algorithm.
static std::size_t
reopen_stdout(void)
{
    const int fd = ::open("/dev/tty", O_WRONLY);
    if (fd == -1)
        ATF_SKIP(F("Cannot open tty for test: %s") % ::strerror(errno));
    struct ::winsize ws;
    if (::ioctl(fd, TIOCGWINSZ, &ws) == -1)
        ATF_SKIP(F("Cannot determine size of tty: %s") % ::strerror(errno));

    if (fd != STDOUT_FILENO) {
        if (::dup2(fd, STDOUT_FILENO) == -1)
            ATF_SKIP(F("Failed to redirect stdout: %s") % ::strerror(errno));
        ::close(fd);
    }

    return ws.ws_col >= 80 ? ws.ws_col - 5 : ws.ws_col;
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(ui__screen_width__columns_set__no_tty);
ATF_TEST_CASE_BODY(ui__screen_width__columns_set__no_tty)
{
    utils::setenv("COLUMNS", "4321");
    ::close(STDOUT_FILENO);

    cmdline::ui ui;
    ATF_REQUIRE_EQ(4321 - 5, ui.screen_width().get());
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__screen_width__columns_set__tty);
ATF_TEST_CASE_BODY(ui__screen_width__columns_set__tty)
{
    utils::setenv("COLUMNS", "4321");
    (void)reopen_stdout();

    cmdline::ui ui;
    ATF_REQUIRE_EQ(4321 - 5, ui.screen_width().get());
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__screen_width__columns_empty__no_tty);
ATF_TEST_CASE_BODY(ui__screen_width__columns_empty__no_tty)
{
    utils::setenv("COLUMNS", "");
    ::close(STDOUT_FILENO);

    cmdline::ui ui;
    ATF_REQUIRE(!ui.screen_width());
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__screen_width__columns_empty__tty);
ATF_TEST_CASE_BODY(ui__screen_width__columns_empty__tty)
{
    utils::setenv("COLUMNS", "");
    const std::size_t columns = reopen_stdout();

    cmdline::ui ui;
    ATF_REQUIRE_EQ(columns, ui.screen_width().get());
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__screen_width__columns_invalid__no_tty);
ATF_TEST_CASE_BODY(ui__screen_width__columns_invalid__no_tty)
{
    utils::setenv("COLUMNS", "foo bar");
    ::close(STDOUT_FILENO);

    cmdline::ui ui;
    ATF_REQUIRE(!ui.screen_width());
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__screen_width__columns_invalid__tty);
ATF_TEST_CASE_BODY(ui__screen_width__columns_invalid__tty)
{
    utils::setenv("COLUMNS", "foo bar");
    const std::size_t columns = reopen_stdout();

    cmdline::ui ui;
    ATF_REQUIRE_EQ(columns, ui.screen_width().get());
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__screen_width__tty_is_file);
ATF_TEST_CASE_BODY(ui__screen_width__tty_is_file)
{
    utils::unsetenv("COLUMNS");
    const int fd = ::open("test.txt", O_WRONLY | O_CREAT | O_TRUNC);
    ATF_REQUIRE(fd != -1);
    if (fd != STDOUT_FILENO) {
        ATF_REQUIRE(::dup2(fd, STDOUT_FILENO) != -1);
        ::close(fd);
    }

    cmdline::ui ui;
    ATF_REQUIRE(!ui.screen_width());
}


ATF_TEST_CASE_WITHOUT_HEAD(ui__screen_width__cached);
ATF_TEST_CASE_BODY(ui__screen_width__cached)
{
    cmdline::ui ui;

    utils::setenv("COLUMNS", "100");
    ATF_REQUIRE_EQ(100 - 5, ui.screen_width().get());

    utils::setenv("COLUMNS", "80");
    ATF_REQUIRE_EQ(100 - 5, ui.screen_width().get());

    utils::unsetenv("COLUMNS");
    ATF_REQUIRE_EQ(100 - 5, ui.screen_width().get());
}


ATF_TEST_CASE_WITHOUT_HEAD(print_error);
ATF_TEST_CASE_BODY(print_error)
{
    cmdline::init("error-program");
    ui_test ui;
    cmdline::print_error(&ui, "The error");
    ATF_REQUIRE(ui.out_message.empty());
    ATF_REQUIRE_EQ("error-program: E: The error.", ui.err_message);
}


ATF_TEST_CASE_WITHOUT_HEAD(print_info);
ATF_TEST_CASE_BODY(print_info)
{
    cmdline::init("info-program");
    ui_test ui;
    cmdline::print_info(&ui, "The info");
    ATF_REQUIRE(ui.out_message.empty());
    ATF_REQUIRE_EQ("info-program: I: The info.", ui.err_message);
}


ATF_TEST_CASE_WITHOUT_HEAD(print_warning);
ATF_TEST_CASE_BODY(print_warning)
{
    cmdline::init("warning-program");
    ui_test ui;
    cmdline::print_warning(&ui, "The warning");
    ATF_REQUIRE(ui.out_message.empty());
    ATF_REQUIRE_EQ("warning-program: W: The warning.", ui.err_message);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, ui__screen_width__columns_set__no_tty);
    ATF_ADD_TEST_CASE(tcs, ui__screen_width__columns_set__tty);
    ATF_ADD_TEST_CASE(tcs, ui__screen_width__columns_empty__no_tty);
    ATF_ADD_TEST_CASE(tcs, ui__screen_width__columns_empty__tty);
    ATF_ADD_TEST_CASE(tcs, ui__screen_width__columns_invalid__no_tty);
    ATF_ADD_TEST_CASE(tcs, ui__screen_width__columns_invalid__tty);
    ATF_ADD_TEST_CASE(tcs, ui__screen_width__tty_is_file);
    ATF_ADD_TEST_CASE(tcs, ui__screen_width__cached);

    ATF_ADD_TEST_CASE(tcs, print_error);
    ATF_ADD_TEST_CASE(tcs, print_info);
    ATF_ADD_TEST_CASE(tcs, print_warning);
}
