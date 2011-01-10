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

#include <cstdlib>
#include <iostream>

#include "utils/sanity.hpp"


namespace {


/// Returns a textual representation of an assertion type.
///
/// The textual representation is user facing.
///
/// \param type The type of the assertion.  If the type is unknown for whatever
///     reason, a special message is returned.  The code cannot abort in such a
///     case because this code is dealing for assertion errors.
static std::string
format_type(const utils::assert_type type)
{
    switch (type) {
    case utils::invariant: return "Invariant check failed";
    case utils::postcondition: return "Postcondition check failed";
    case utils::precondition: return "Precondition check failed";
    case utils::unreachable: return "Unreachable point reached";
    default: return "UNKNOWN ASSERTION TYPE";
    }
}


}  // anonymous namespace


/// Raises an assertion error.
///
/// This function prints information about the assertion failure and terminates
/// execution immediately by calling std::abort().  This ensures a coredump so
/// that the failure can be analyzed later.
///
/// \param type The assertion type; this influences the printed message.
/// \param file The file in which the assertion failed.
/// \param line The line in which the assertion failed.
/// \param message The failure message associated to the condition.
void
utils::sanity_failure(const utils::assert_type type, const char* file,
                      const size_t line, const std::string& message)
{
    std::cerr << "*** " << file << ":" << line << ": " << format_type(type);
    if (!message.empty())
        std::cerr << ": " << message << "\n";
    else
        std::cerr << "\n";
    std::abort();
}
