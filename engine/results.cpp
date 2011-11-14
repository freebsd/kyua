// Copyright 2010, 2011 Google Inc.
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

#include "engine/results.ipp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/optional.ipp"

namespace results = engine::results;

using utils::none;
using utils::optional;


/// Internal implementation of a result.
struct engine::results::base_result::base_impl {
    /// Whether the result can be considered "good" or not.
    bool good;

    /// The explanation for the result.
    ///
    /// This should be non-none only if the result type supports a reason.
    optional< std::string > reason;

    /// Constructor.
    ///
    /// \param good_ True if the result is good, false otherwise.
    /// \param reason_ The explanation for the result, if any.
    base_impl(const bool good_, const optional< std::string >& reason_) :
        good(good_),
        reason(reason_)
    {
    }
};


/// Constructs a base result.
results::base_result::base_result(const bool good_,
                                  const optional< std::string >& reason_) :
    _pbimpl(new base_impl(good_, reason_))
{
}


/// Destructor for a test result.
results::base_result::~base_result(void)
{
}


/// Gets the reason behind the result, if any.
///
/// \return The reason, or none if not defined.
const optional< std::string >&
results::base_result::optional_reason(void) const
{
    return _pbimpl->reason;
}


/// True if the test case result has a positive connotation.
///
/// \return Whether the test case is good or not.
bool
results::base_result::good(void) const
{
    return _pbimpl->good;
}


/// Constructs a new broken result.
///
/// \param reason_ The reason.
results::broken::broken(const std::string& reason_) :
    base_result(false, utils::make_optional(reason_))
{
}


/// Equality comparator.
///
/// \param other The result to compare to.
///
/// \return True if equal, false otherwise.
bool
results::broken::operator==(const results::broken& other)
    const
{
    return reason() == other.reason();
}


/// Inquality comparator.
///
/// \param other The result to compare to.
///
/// \return True if differed, false otherwise.
bool
results::broken::operator!=(const results::broken& other)
    const
{
    return reason() != other.reason();
}


/// Gets the reason describing the result.
///
/// \return The test-provided reason for the result.
const std::string&
results::broken::reason(void) const
{
    return optional_reason().get();
}


/// Generates a user-friendly representation of the result.
///
/// \return A text-based representation of the result.
///
/// \todo This should be in the 'cli' layer, not in 'engine'.
std::string
results::broken::format(void) const
{
    return F("broken: %s") % reason();
}


/// Constructs a new expected_failure result.
///
/// \param reason_ The reason.
results::expected_failure::expected_failure(const std::string& reason_) :
    base_result(true, utils::make_optional(reason_))
{
}


/// Equality comparator.
///
/// \param other The result to compare to.
///
/// \return True if equal, false otherwise.
bool
results::expected_failure::operator==(const results::expected_failure& other)
    const
{
    return reason() == other.reason();
}


/// Inquality comparator.
///
/// \param other The result to compare to.
///
/// \return True if differed, false otherwise.
bool
results::expected_failure::operator!=(const results::expected_failure& other)
    const
{
    return reason() != other.reason();
}


/// Gets the reason describing the result.
///
/// \return The test-provided reason for the result.
const std::string&
results::expected_failure::reason(void) const
{
    return optional_reason().get();
}


/// Generates a user-friendly representation of the result.
///
/// \return A text-based representation of the result.
///
/// \todo This should be in the 'cli' layer, not in 'engine'.
std::string
results::expected_failure::format(void) const
{
    return F("expected_failure: %s") % reason();
}


/// Constructs a new failed result.
///
/// \param reason_ The reason.
results::failed::failed(const std::string& reason_) :
    base_result(false, utils::make_optional(reason_))
{
}


/// Equality comparator.
///
/// \param other The result to compare to.
///
/// \return True if equal, false otherwise.
bool
results::failed::operator==(const results::failed& other)
    const
{
    return reason() == other.reason();
}


/// Inquality comparator.
///
/// \param other The result to compare to.
///
/// \return True if differed, false otherwise.
bool
results::failed::operator!=(const results::failed& other)
    const
{
    return reason() != other.reason();
}


/// Gets the reason describing the result.
///
/// \return The test-provided reason for the result.
const std::string&
results::failed::reason(void) const
{
    return optional_reason().get();
}


/// Generates a user-friendly representation of the result.
///
/// \return A text-based representation of the result.
///
/// \todo This should be in the 'cli' layer, not in 'engine'.
std::string
results::failed::format(void) const
{
    return F("failed: %s") % reason();
}


/// Constructs a new passed result.
results::passed::passed(void) :
    base_result(true, none)
{
}


/// Equality comparator.
///
/// \param unused_other The result to compare to.
///
/// \return True if equal, false otherwise.
bool
results::passed::operator==(const results::passed& UTILS_UNUSED_PARAM(other))
    const
{
    return true;
}


/// Inquality comparator.
///
/// \param unused_other The result to compare to.
///
/// \return True if differed, false otherwise.
bool
results::passed::operator!=(const results::passed& UTILS_UNUSED_PARAM(other))
    const
{
    return false;
}


/// Generates a user-friendly representation of the result.
///
/// \return A text-based representation of the result.
///
/// \todo This should be in the 'cli' layer, not in 'engine'.
std::string
results::passed::format(void) const
{
    return "passed";
}


/// Constructs a new skipped result.
///
/// \param reason_ The reason.
results::skipped::skipped(const std::string& reason_) :
    base_result(true, utils::make_optional(reason_))
{
}


/// Equality comparator.
///
/// \param other The result to compare to.
///
/// \return True if equal, false otherwise.
bool
results::skipped::operator==(const results::skipped& other)
    const
{
    return reason() == other.reason();
}


/// Inquality comparator.
///
/// \param other The result to compare to.
///
/// \return True if differed, false otherwise.
bool
results::skipped::operator!=(const results::skipped& other)
    const
{
    return reason() != other.reason();
}


/// Gets the reason describing the result.
///
/// \return The test-provided reason for the result.
const std::string&
results::skipped::reason(void) const
{
    return optional_reason().get();
}


/// Generates a user-friendly representation of the result.
///
/// \return A text-based representation of the result.
///
/// \todo This should be in the 'cli' layer, not in 'engine'.
std::string
results::skipped::format(void) const
{
    return F("skipped: %s") % reason();
}
