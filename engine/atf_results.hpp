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

/// \file engine/atf_results.hpp
/// Functions and types to process the results of ATF-based test cases.

#if !defined(ENGINE_ATF_RESULTS_HPP)
#define ENGINE_ATF_RESULTS_HPP

#include <istream>

#include "engine/results.hpp"
#include "utils/datetime.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.hpp"
#include "utils/process/status.hpp"

namespace engine {

class atf_test_case;

namespace results {


result_ptr parse(std::istream&);
result_ptr load(const utils::fs::path&);
result_ptr adjust_with_status(result_ptr, const utils::process::status&);
result_ptr adjust_with_timeout(result_ptr, const utils::datetime::delta&);
result_ptr adjust(const engine::atf_test_case&,
                  const utils::optional< utils::process::status >&,
                  const utils::optional< utils::process::status >&,
                  result_ptr);


}  // namespace results
}  // namespace engine


#endif  // !defined(ENGINE_ATF_RESULTS_HPP)
