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

/// \file engine/atf_test_case.hpp
/// Provides the atf_test_case class and other auxiliary types.

#if !defined(ENGINE_ATF_TEST_CASE_HPP)
#define ENGINE_ATF_TEST_CASE_HPP

#include <map>
#include <set>
#include <string>

#include "engine/test_case.hpp"
#include "engine/user_files/config.hpp"
#include "utils/datetime.hpp"
#include "utils/fs/path.hpp"

namespace engine {


/// Collection of paths.
typedef std::set< utils::fs::path > paths_set;


/// Collection of strings.
typedef std::set< std::string > strings_set;


namespace detail {


bool parse_bool(const std::string&, const std::string&);
strings_set parse_list(const std::string&, const std::string&);
unsigned long parse_ulong(const std::string&, const std::string&);

paths_set parse_require_files(const std::string&, const std::string&);
paths_set parse_require_progs(const std::string&, const std::string&);
std::string parse_require_user(const std::string&, const std::string&);


}  // namespace detail


/// Representation of an ATF test case.
///
/// Test cases should be thought as free-standing entities: even though they
/// are located within a test program, the test program serves no other purpose
/// than to provide a way to execute the test cases.  Therefore, no information
/// needs to be stored for the test programs themselves.
class atf_test_case : public test_case {
    properties_map get_all_properties(void) const;
    results::result_ptr do_run(const user_files::config&) const;

public:
    /// The test case description.
    std::string description;

    /// Whether the test case has a cleanup routine or not.
    bool has_cleanup;

    /// The maximum amount of time the test case can run for.
    utils::datetime::delta timeout;

    /// List of architectures in which the test case can run; empty = any.
    strings_set allowed_architectures;

    /// List of platforms in which the test case can run; empty = any.
    strings_set allowed_platforms;

    /// List of configuration variables needed by the test case.
    strings_set required_configs;

    /// List of files needed by the test case.
    paths_set required_files;

    /// List of programs needed by the test case.
    paths_set required_programs;

    /// Privileges required to run the test case.
    ///
    /// Can be empty, in which case means "any privileges", or any of "root" or
    /// "unprivileged".
    std::string required_user;

    /// User-defined meta-data properties.
    properties_map user_metadata;

    atf_test_case(const engine::test_program&, const std::string&,
                  const std::string&, const bool,
                  const utils::datetime::delta&, const strings_set&,
                  const strings_set&, const strings_set&, const paths_set&,
                  const paths_set&, const std::string&, const properties_map&);

    static atf_test_case from_properties(const engine::test_program&,
                                         const std::string&,
                                         const properties_map&);

    std::string check_requirements(const user_files::config&) const;

    bool operator==(const atf_test_case&) const;
};


}  // namespace engine


#endif  // !defined(ENGINE_ATF_TEST_CASE_HPP)
