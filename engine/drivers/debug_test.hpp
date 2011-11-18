// Copyright 2011 Google Inc.
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

/// \file engine/drivers/debug_test.hpp
/// Driver to run a single test in a controlled manner.
///
/// This driver module implements the logic to execute a particular test
/// with hooks into the runtime procedure.  This is to permit debugging the
/// behavior of the test.

#if !defined(ENGINE_DRIVERS_DEBUG_TEST_HPP)
#define ENGINE_DRIVERS_DEBUG_TEST_HPP

#include "engine/filters.hpp"
#include "engine/test_case.hpp"
#include "engine/test_result.hpp"
#include "engine/user_files/config.hpp"
#include "utils/fs/path.hpp"

namespace engine {
namespace drivers {
namespace debug_test {


/// Tuple containing the results of this driver.
struct result {
    /// The identifier of the executed test case.
    test_case_id test_id;

    /// The result of the test case.
    engine::test_result test_result;

    /// Initializer for the tuple's fields.
    ///
    /// \param test_id_ The identifier of the test case.
    /// \param test_result_ The result of the test case.
    result(const test_case_id& test_id_,
           const engine::test_result& test_result_) :
        test_id(test_id_),
        test_result(test_result_)
    {
    }
};


result drive(const utils::fs::path&, const test_filter&,
             const user_files::config&, const utils::fs::path&,
             const utils::fs::path&);


}  // namespace debug_test
}  // namespace drivers
}  // namespace engine

#endif  // !defined(ENGINE_DRIVERS_DEBUG_TEST_HPP)
