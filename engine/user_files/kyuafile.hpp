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

/// \file engine/user_files/kyuafile.hpp
/// Test suite configuration parsing and representation.

#if !defined(ENGINE_USER_FILES_KYUAFILE_HPP)
#define ENGINE_USER_FILES_KYUAFILE_HPP

#include <string>
#include <vector>

#include <lutok/wrap.hpp>

#include "engine/test_program.hpp"
#include "utils/fs/path.hpp"

namespace engine {
namespace user_files {


namespace detail {


test_program_ptr get_test_program(lutok::state&, const utils::fs::path&);
test_programs_vector get_test_programs(lutok::state&, const std::string&,
                                       const utils::fs::path&);


}  // namespace detail


/// Representation of the configuration of a test suite.
///
/// Test suites are collections of related test programs.  They are described by
/// a configuration file.
///
/// This class provides the parser for test suite configuration files and
/// methods to access the parsed data.
class kyuafile {
    utils::fs::path _root;
    test_programs_vector _test_programs;

public:
    explicit kyuafile(const utils::fs::path&, const test_programs_vector&);
    static kyuafile load(const utils::fs::path&);

    const utils::fs::path& root(void) const;
    const test_programs_vector& test_programs(void) const;
};


}  // namespace user_files
}  // namespace engine

#endif  // !defined(ENGINE_USER_FILES_KYUAFILE_HPP)
