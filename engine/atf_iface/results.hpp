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

/// \file engine/atf_iface/results.hpp
/// Functions and types to process the results of ATF-based test cases.

#if !defined(ENGINE_ATF_IFACE_RESULTS_HPP)
#define ENGINE_ATF_IFACE_RESULTS_HPP

#include <istream>

#include "utils/datetime.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.hpp"
#include "utils/process/status.hpp"

namespace engine {
class test_result;
namespace atf_iface {


namespace detail {


/// Internal representation of the raw result files of ATF-based tests.
///
/// This class is used exclusively to represent the transient result files read
/// from test cases before generating the "public" version of the result.  This
/// class should actually not be exposed in the header files, but it is for
/// testing purposes only.
class raw_result {
public:
    /// List of possible types for the test case result.
    enum types {
        broken,
        expected_death,
        expected_exit,
        expected_failure,
        expected_signal,
        expected_timeout,
        failed,
        passed,
        skipped,
    };

private:
    types _type;
    utils::optional< int > _argument;
    utils::optional< std::string > _reason;

public:
    raw_result(const types);
    raw_result(const types, const std::string&);
    raw_result(const types, const utils::optional< int >&, const std::string&);

    static raw_result parse(std::istream&);
    static raw_result load(const utils::fs::path&);

    types type(void) const;
    const utils::optional< int >& argument(void) const;
    const utils::optional< std::string >& reason(void) const;

    bool good(void) const;
    raw_result apply(const utils::optional< utils::process::status >&) const;
    test_result externalize(void) const;

    bool operator==(const raw_result&) const;
};


}  // namespace detail


test_result calculate_result(
    const utils::optional< utils::process::status >&,
    const utils::optional< utils::process::status >&,
    const utils::fs::path&);


}  // namespace atf_iface
}  // namespace engine


#endif  // !defined(ENGINE_ATF_IFACE_RESULTS_HPP)
