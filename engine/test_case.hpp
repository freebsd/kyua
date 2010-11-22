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

/// \file engine/test_case.hpp
/// Provides the test_case class and other auxiliary types.

#if !defined(ENGINE_TEST_CASE_HPP)
#define ENGINE_TEST_CASE_HPP

#include <map>
#include <string>

#include "utils/fs/path.hpp"

namespace engine {


/// Collection of test case properties.
///
/// A property is just a (name, value) pair, and we represent them as a map
/// because callers always want to locate properties by name.
typedef std::map< std::string, std::string > properties_map;


/// Representation of a test case.
///
/// Test cases should be thought as free-standing entities: even though they
/// are located within a test program, the test program serves no other purpose
/// than to provide a way to execute the test cases.  Therefore, no information
/// needs to be stored for the test programs themselves.
class test_case {
    utils::fs::path _program;
    std::string _name;
    properties_map _metadata;

public:
    test_case(const utils::fs::path&, const std::string&,
              const properties_map&);

    const utils::fs::path& program(void) const;
    const std::string& name(void) const;
    const properties_map& metadata(void) const;

    bool operator==(const test_case&) const;
};


}  // namespace engine


#endif  // !defined(ENGINE_TEST_CASE_HPP)
