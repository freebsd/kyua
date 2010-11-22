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

#include <istream>
#include <string>

#include "utils/fs/path.hpp"
#include "utils/optional.hpp"
#include "utils/process/status.hpp"

namespace engine {
namespace results {


/// Base abstract class to represent a test case result.
struct base_result {
    virtual ~base_result(void) = 0;

    /// Simple formatter.
    ///
    /// \return The formatted result.
    virtual std::string format(void) const = 0;
};


// TODO(jmmv): It probably makes sense to create a typedef for this auto_ptr.
std::auto_ptr< const base_result > parse(std::istream&);
std::auto_ptr< const base_result > load(const utils::fs::path&);
std::auto_ptr< const base_result > adjust(std::auto_ptr< const base_result >,
                                          const utils::process::status&,
                                          const bool);


/// Representation of a broken test case.
struct broken : public base_result {
    /// The reason for the brokenness.
    std::string reason;

    broken(const std::string&);
    bool operator==(const broken&) const;
    bool operator!=(const broken&) const;

    std::string format(void) const;
};


/// Representation of a test case that expectedly dies.
struct expected_death : public base_result {
    /// The reason for the expected death.
    std::string reason;

    expected_death(const std::string&);
    bool operator==(const expected_death&) const;
    bool operator!=(const expected_death&) const;

    std::string format(void) const;
};


/// Representation of a test case that expectedly exits.
struct expected_exit : public base_result {
    /// The expected exit code; if none, any exit code is valid.
    utils::optional< int > exit_status;

    /// The reason for the expected controlled exit.
    std::string reason;

    expected_exit(const utils::optional< int >&, const std::string&);
    bool operator==(const expected_exit&) const;
    bool operator!=(const expected_exit&) const;

    std::string format(void) const;
};


/// Representation of a test case that expectedly fails.
struct expected_failure : public base_result {
    /// The reason for the expected failure.
    std::string reason;

    expected_failure(const std::string&);
    bool operator==(const expected_failure&) const;
    bool operator!=(const expected_failure&) const;

    std::string format(void) const;
};


/// Representation of a test case that expectedly receives a signal.
struct expected_signal : public base_result {
    /// The expected signal number; if none, any signal is valid.
    utils::optional< int > signal_no;

    /// The reason for the expected signal delivery.
    std::string reason;

    expected_signal(const utils::optional< int >&, const std::string&);
    bool operator==(const expected_signal&) const;
    bool operator!=(const expected_signal&) const;

    std::string format(void) const;
};


/// Representation of a test case that expectedly times out.
struct expected_timeout : public base_result {
    /// The reason for the expected timeout.
    std::string reason;

    expected_timeout(const std::string&);
    bool operator==(const expected_timeout&) const;
    bool operator!=(const expected_timeout&) const;

    std::string format(void) const;
};


/// Representation of a test case that fails.
struct failed : public base_result {
    /// The reason for the failure.
    std::string reason;

    failed(const std::string&);
    bool operator==(const failed&) const;
    bool operator!=(const failed&) const;

    std::string format(void) const;
};


/// Representation of a test case that succeeds.
struct passed : public base_result {
    passed(void);
    bool operator==(const passed&) const;
    bool operator!=(const passed&) const;

    std::string format(void) const;
};


/// Representation of a test case that is skipped.
struct skipped : public base_result {
    /// The reason for the skipping.
    std::string reason;

    skipped(const std::string&);
    bool operator==(const skipped&) const;
    bool operator!=(const skipped&) const;

    std::string format(void) const;
};


}  // namespace results
}  // namespace engine


#endif  // !defined(ENGINE_RESULTS_HPP)
