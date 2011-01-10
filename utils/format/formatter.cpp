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

#include <string>

#include "utils/format/exceptions.hpp"
#include "utils/format/formatter.hpp"
#include "utils/sanity.hpp"

using utils::format::bad_format_error;
using utils::format::extra_args_error;
using utils::format::formatter;


static std::string valid_formatters = "cdsu%";


/// Constructs a new formatter object (internal).
///
/// \param format The format string.
/// \param expansion The format string with any replacements performed so far.
/// \param last_pos The position from which to start looking for formatting
///     placeholders.  This must be maintained in case one of the replacements
///     introduced a new placeholder, which must be ignored.  Think, for
///     example, replacing a "%s" string with "foo %s".
formatter::formatter(const std::string& format, const std::string& expansion,
                     const std::string::size_type last_pos) :
    _format(format),
    _expansion(expansion),
    _last_pos(last_pos)
{
}


/// Constructs a new formatter object.
///
/// \param format The format string.
///
/// \throw utils::format::bad_format_error If the format string is invalid.
formatter::formatter(const std::string& format) :
    _format(format),
    _expansion(format),
    _last_pos(0)
{
    std::string::size_type pos = 0;
    while (pos < _format.length()) {
        if (_format[pos] == '%') {
            if (pos == _format.length() - 1) {
                throw bad_format_error(_format, "Trailing %");
            } else {
                pos++;
                if (valid_formatters.find(_format[pos]) == std::string::npos)
                    throw bad_format_error(_format, "Unknown sequence '%'" +
                                           _format.substr(pos, 1) + "'");
            }
        }
        pos++;
    }
}

/// Returns the formatted string.
std::string
formatter::str(void) const
{
    std::string out = _expansion;

    std::string::size_type pos = out.find("%%");
    while (pos != std::string::npos) {
        out.erase(pos, 1);
        pos = out.find("%%", pos);
    }

    return out;
}


/// Automatic conversion of formatter objects to strings.
///
/// This is provided to allow painless injection of formatter objects into
/// streams, without having to manually call the str() method.
formatter::operator std::string(void) const
{
    return str();
}


/// Replaces the first formatting placeholder with a value.
///
/// \param arg The replacement string.
///
/// \return A new formatter in which the first formatting placeholder has been
///     replaced by arg and is ready to replace the next item.
///
/// \throw utils::format::extra_args_error If there are no more formatting
///     placeholders in the input string.
formatter
formatter::replace(const std::string& arg) const
{
    std::string::size_type pos = _expansion.find('%', _last_pos);
    while (pos != std::string::npos) {
        INV(pos < _expansion.length() - 1);
        if (_expansion[pos + 1] != '%')
            break;
        pos = _expansion.find('%', pos + 2);
    }
    if (pos == std::string::npos)
        throw extra_args_error(_format, arg);

    const std::string expansion = _expansion.substr(0, pos) + arg +
        _expansion.substr(pos + 2);
    return formatter(_format, expansion, pos + arg.length());
}
