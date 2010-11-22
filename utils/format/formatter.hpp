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

/// \file utils/format/formatter.hpp
/// Provides the definition of the utils::format::formatter class.
///
/// The utils::format::formatter class is a poor man's replacement for the
/// Boost.Format library, as it is much simpler and has less dependencies.

#if !defined(UTILS_FORMAT_FORMATTER_HPP)
#define UTILS_FORMAT_FORMATTER_HPP

#include <string>

namespace utils {
namespace format {


/// Mechanism to format strings similar to printf.
///
/// A formatter always maintains the original format string but also holds a
/// partial expansion.  The partial expansion is immutable in the context of a
/// formatter instance, but calls to operator% return new formatter objects with
/// one less formatting placeholder.
///
/// In general, one can format a string in the following manner:
///
/// \code
/// const std::string s = (formatter("%s %d") % "foo" % 5).str();
/// \endcode
///
/// which, following the explanation above, would correspond to:
///
/// \code
/// const formatter f1("%s %d");
/// const formatter f2 = f1 % "foo";
/// const formatter f3 = f2 % 5;
/// const std::string s = f3.str();
/// \endcode
class formatter {
    std::string _format;

    std::string _expansion;
    std::string::size_type _last_pos;

    formatter replace(const std::string&) const;

    formatter(const std::string&, const std::string&,
              const std::string::size_type);

public:
    formatter(const std::string&);

    std::string str(void) const;
    operator std::string(void) const;

    template< typename Type > formatter operator%(const Type&) const;
};


}  // namespace format
}  // namespace utils


#endif  // !defined(UTILS_FORMAT_FORMATTER_HPP)
