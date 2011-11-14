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

/// \file engine/results.hpp
/// Representation and parsing of test case results.
///
/// This module provides a set of classes to represent all the possible results
/// of a test case.  These results are represented as different classes because
/// each result may contain a different subset of valid fields (e.g. an optional
/// integer argument or an optional reason).  The overall approach is quite
/// complex but ensures that every result only contains fields it requires, and
/// thus proper validation can be performed at compilation time.
///
/// Note that test cases that generate an invalid test result are considered to
/// be broken (e.g. they do not conform to what we expect here or the test
/// program monitor code is broken).  Therefore, the specific result provided by
/// them (if any) is discarded and is transformed into a results::broken result.
///
/// Users of this module need to downcast results::base_result instances to
/// their specific types for further processing.

#if !defined(ENGINE_RESULTS_HPP)
#define ENGINE_RESULTS_HPP

#include <string>
#include <tr1/memory>

#include "utils/optional.hpp"

namespace engine {
namespace results {


/// Base abstract class to represent a test case result.
///
/// This base class provides common functionality across all result types.  It
/// also provides helper utilities to make the implementation of the subtypes
/// easier; for example, it provides a mechanism to represent result reasons,
/// yet the "passed" type will not want to expose it.
///
/// \todo I think special-casing the "passed" type to not expose a reason is a
/// lot of hassle.  Maybe we should just optionally-return a result regardless
/// (being none if not available).
class base_result {
    struct base_impl;
    std::tr1::shared_ptr< base_impl > _pbimpl;

protected:
    base_result(const bool, const utils::optional< std::string >&);

    const utils::optional< std::string >& optional_reason(void) const;

public:
    virtual ~base_result(void) = 0;

    /// Simple formatter.
    ///
    /// \return The formatted result.
    virtual std::string format(void) const = 0;

    bool good(void) const;
};


/// Shared pointer to a const test case result.
typedef std::tr1::shared_ptr< const base_result > result_ptr;


/// Representation of a broken test case.
class broken : public base_result {
public:
    broken(const std::string&);
    bool operator==(const broken&) const;
    bool operator!=(const broken&) const;

    const std::string& reason(void) const;
    std::string format(void) const;
};


/// Representation of a test case that expectedly fails.
class expected_failure : public base_result {
public:
    expected_failure(const std::string&);
    bool operator==(const expected_failure&) const;
    bool operator!=(const expected_failure&) const;

    const std::string& reason(void) const;
    std::string format(void) const;
};


/// Representation of a test case that fails.
class failed : public base_result {
public:
    failed(const std::string&);
    bool operator==(const failed&) const;
    bool operator!=(const failed&) const;

    const std::string& reason(void) const;
    std::string format(void) const;
};


/// Representation of a test case that succeeds.
class passed : public base_result {
public:
    passed(void);
    bool operator==(const passed&) const;
    bool operator!=(const passed&) const;

    std::string format(void) const;
};


/// Representation of a test case that is skipped.
class skipped : public base_result {
public:
    skipped(const std::string&);
    bool operator==(const skipped&) const;
    bool operator!=(const skipped&) const;

    const std::string& reason(void) const;
    std::string format(void) const;
};


}  // namespace results
}  // namespace engine


#endif  // !defined(ENGINE_RESULTS_HPP)
