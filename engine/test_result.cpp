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

#include "engine/test_result.hpp"
#include "utils/sanity.hpp"


/// Constructs a base result.
///
/// \param type_ The type of the result.
/// \param reason_ The reason explaining the result, if any.  It is OK for this
///     to be empty, which is actually the default.
engine::test_result::test_result(const result_type type_,
                                 const std::string& reason_) :
    _type(type_),
    _reason(reason_)
{
}


/// Returns the type of the result.
///
/// \return A result type.
engine::test_result::result_type
engine::test_result::type(void) const
{
    return _type;
}


/// Returns the reason explaining the result.
///
/// \return A textual reason, possibly empty.
const std::string&
engine::test_result::reason(void) const
{
    return _reason;
}


/// True if the test case result has a positive connotation.
///
/// \return Whether the test case is good or not.
bool
engine::test_result::good(void) const
{
    switch (_type) {
    case expected_failure:
    case passed:
    case skipped:
        return true;

    case broken:
    case failed:
        return false;
    }
    UNREACHABLE;
}


/// Equality comparator.
///
/// \param other The test result to compare to.
///
/// \return True if the other object is equal to this one, false otherwise.
bool
engine::test_result::operator==(const test_result& other) const
{
    return _type == other._type && _reason == other._reason;
}


/// Inequality comparator.
///
/// \param other The test result to compare to.
///
/// \return True if the other object is different from this one, false
/// otherwise.
bool
engine::test_result::operator!=(const test_result& other) const
{
    return !(*this == other);
}
