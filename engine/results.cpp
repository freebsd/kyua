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

using utils::optional;


/// Destructor for a test result.
results::base_result::~base_result(void)
{
}


/// Constructs a new broken result.
///
/// \param reason_ The reason.
results::broken::broken(const std::string& reason_) :
    reason(reason_)
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
    return reason == other.reason;
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
    return reason != other.reason;
}


std::string
results::broken::format(void) const
{
    return F("broken: %s") % reason;
}


bool
results::broken::good(void) const
{
    return false;
}


/// Constructs a new expected_failure result.
///
/// \param reason_ The reason.
results::expected_failure::expected_failure(const std::string& reason_) :
    reason(reason_)
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
    return reason == other.reason;
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
    return reason != other.reason;
}


std::string
results::expected_failure::format(void) const
{
    return F("expected_failure: %s") % reason;
}


bool
results::expected_failure::good(void) const
{
    return true;
}


/// Constructs a new failed result.
///
/// \param reason_ The reason.
results::failed::failed(const std::string& reason_) :
    reason(reason_)
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
    return reason == other.reason;
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
    return reason != other.reason;
}


std::string
results::failed::format(void) const
{
    return F("failed: %s") % reason;
}


bool
results::failed::good(void) const
{
    return false;
}


/// Constructs a new passed result.
results::passed::passed(void)
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


std::string
results::passed::format(void) const
{
    return "passed";
}


bool
results::passed::good(void) const
{
    return true;
}


/// Constructs a new skipped result.
///
/// \param reason_ The reason.
results::skipped::skipped(const std::string& reason_) :
    reason(reason_)
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
    return reason == other.reason;
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
    return reason != other.reason;
}


std::string
results::skipped::format(void) const
{
    return F("skipped: %s") % reason;
}


bool
results::skipped::good(void) const
{
    return true;
}
