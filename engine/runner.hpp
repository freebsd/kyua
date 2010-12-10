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

/// \file engine/runner.hpp
/// Test suite execution.

#if !defined(ENGINE_RUNNER_HPP)
#define ENGINE_RUNNER_HPP

#include <string>

#include "engine/results.hpp"
#include "engine/test_case.hpp"
#include "utils/fs/path.hpp"


namespace engine {


class suite_config;
struct test_case;


namespace runner {


/// Callbacks for the execution of test suites and programs.
class hooks {
public:
    virtual ~hooks(void) = 0;

    /// Hook called right before a test case is executed.
    ///
    /// \param identifier The test case identifier.
    virtual void start_test_case(const test_case_id& identifier) = 0;

    /// Hook called right after a test case is executed.
    ///
    /// \param identifier The test case identifier.
    /// \param result The result of the test case.  To grab ownership of this
    ///     pointer, just use release() on the smart pointer.
    virtual void finish_test_case(
        const test_case_id& identifier,
        std::auto_ptr< const results::base_result > result) = 0;
};


std::auto_ptr< const results::base_result > run_test_case(
    const engine::test_case&, const properties_map&);
void run_test_program(const utils::fs::path&, const properties_map&, hooks*);
void run_test_suite(const engine::suite_config&, const properties_map&, hooks*);


}  // namespace runner
}  // namespace engine

#endif  // !defined(ENGINE_RUNNER_HPP)
