// Copyright 2014 Google Inc.
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

/// \file engine/report_junit.hpp
/// Generates a JUnit report out of a test suite execution.

#if !defined(ENGINE_REPORT_JUNIT_HPP)
#define ENGINE_REPORT_JUNIT_HPP

#include <ostream>
#include <string>

#include "engine/drivers/scan_results.hpp"

namespace utils {
namespace datetime {
class delta;
}  // namespace datetime
}  // namespace utils

namespace engine {


class metadata;
class test_program;


std::string junit_classname(const engine::test_program&);
std::string junit_duration(const utils::datetime::delta&);
extern const char* const junit_metadata_prefix;
extern const char* const junit_metadata_suffix;
std::string junit_metadata(const engine::metadata&);


/// Hooks for the scan_results driver to generate a JUnit report.
class report_junit_hooks : public engine::drivers::scan_results::base_hooks {
    /// Stream to which to write the report.
    std::ostream& _output;

public:
    report_junit_hooks(std::ostream&);

    void got_context(const engine::context&);
    void got_result(store::results_iterator&);

    void end(const engine::drivers::scan_results::result&);
};


}  // namespace engine

#endif  // !defined(ENGINE_REPORT_JUNIT_HPP)
