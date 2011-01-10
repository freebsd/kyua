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

/// \file utils/test_utils.hpp
/// Helper utilities for test programs.
///
/// The routines provided in this file are only supposed to be used from test
/// programs.  They rely on atf-c++ being linked in and assume they are being
/// called from within test cases.  In particular, none of these routines bother
/// to report errors to the caller: any internal, unexpected error causes the
/// test case to fail immediately.

#if !defined(UTILS_TEST_UTILS_HPP)
#define UTILS_TEST_UTILS_HPP

#include <string>
#include <vector>

#include "utils/fs/path.hpp"

namespace utils {


void cat_file(const std::string&, const fs::path&);
void create_file(const fs::path&);
bool grep_file(const std::string&, const fs::path&);
bool grep_string(const std::string&, const std::string&);
bool grep_vector(const std::string&, const std::vector< std::string >&);


}  // namespace utils

#endif  // !defined(UTILS_TEST_UTILS_HPP)
