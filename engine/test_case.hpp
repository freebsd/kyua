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

/// \file engine/test_case.hpp
/// Provides the test_case class and other auxiliary types.

#if !defined(ENGINE_TEST_CASE_HPP)
#define ENGINE_TEST_CASE_HPP

#include <map>
#include <set>
#include <string>

#include "engine/user_files/config.hpp"
#include "utils/datetime.hpp"
#include "utils/fs/path.hpp"

namespace engine {


/// Collection of test case properties.
///
/// A property is just a (name, value) pair, and we represent them as a map
/// because callers always want to locate properties by name.
typedef std::map< std::string, std::string > properties_map;


/// Collection of paths.
typedef std::set< utils::fs::path > paths_set;


/// Collection of strings.
typedef std::set< std::string > strings_set;


namespace detail {


bool parse_bool(const std::string&, const std::string&);
strings_set parse_list(const std::string&, const std::string&);
unsigned long parse_ulong(const std::string&, const std::string&);

paths_set parse_require_progs(const std::string&, const std::string&);
std::string parse_require_user(const std::string&, const std::string&);


}  // namespace detail


/// Representation of a test case identifier.
///
/// A test case identifier is a unique value that identifies the test case
/// inside a particular test suite.  Given that the program is only supposed to
/// deal with one test suite at a time, we can assume that the test case
/// identifier is unique within the program.
struct test_case_id {
    /// Name of the test program containing the test case.
    utils::fs::path program;

    /// Name of the test case within the test program.
    std::string name;

    test_case_id(const utils::fs::path&, const std::string&);

    std::string str(void) const;

    bool operator<(const test_case_id&) const;
    bool operator==(const test_case_id&) const;
};


/// Representation of a test case.
///
/// Test cases should be thought as free-standing entities: even though they
/// are located within a test program, the test program serves no other purpose
/// than to provide a way to execute the test cases.  Therefore, no information
/// needs to be stored for the test programs themselves.
struct test_case {
    /// The test case identifier.
    test_case_id identifier;

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

    /// List of programs needed by the test case.
    paths_set required_programs;

    /// Privileges required to run the test case.
    ///
    /// Can be empty, in which case means "any privileges", or any of "root" or
    /// "unprivileged".
    std::string required_user;

    /// User-defined meta-data properties.
    properties_map user_metadata;

    test_case(const test_case_id&, const std::string&, const bool,
              const utils::datetime::delta&, const strings_set&,
              const strings_set&, const strings_set&, const paths_set&,
              const std::string&, const properties_map&);

    static test_case from_properties(const test_case_id&,
                                     const properties_map&);

    bool operator==(const test_case&) const;
};


std::string check_requirements(const test_case&, const user_files::config&,
                               const properties_map&);


}  // namespace engine


#endif  // !defined(ENGINE_TEST_CASE_HPP)
