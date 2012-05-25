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
/// Interface to interact with test cases.

#if !defined(ENGINE_TEST_CASE_HPP)
#define ENGINE_TEST_CASE_HPP

#include <map>
#include <string>
#include <tr1/memory>

#include "utils/fs/path.hpp"
#include "utils/optional.hpp"

namespace engine {


class test_result;


/// Collection of test case properties.
///
/// A property is just a (name, value) pair, and we represent them as a map
/// because callers always want to locate properties by name.
typedef std::map< std::string, std::string > properties_map;


class base_test_program;

namespace user_files {
class config;
}  // namespace user_files


/// Hooks to introspect the execution of a test case.
///
/// There is no guarantee that these hooks will be called during the execution
/// of the test case.  There are conditions in which they don't make sense.
///
/// Note that this class is not abstract.  All hooks have default, empty
/// implementations.  The purpose of this is to simplify some tests that need to
/// pass hooks but that are not interested in the results.  We might want to
/// rethink this and provide an "empty subclass" of a base abstract template.
class test_case_hooks {
public:
    virtual ~test_case_hooks(void);

    virtual void got_stdout(const utils::fs::path&);
    virtual void got_stderr(const utils::fs::path&);
};


/// Representation of a test case.
class base_test_case {
    struct base_impl;

    /// Pointer to the shared internal implementation.
    std::tr1::shared_ptr< base_impl > _pbimpl;

    /// Returns a string representation of all test case properties.
    ///
    /// The returned keys and values match those that can be defined by the test
    /// case.
    ///
    /// \return A key/value mapping describing all the test case properties.
    virtual properties_map get_all_properties(void) const = 0;

    /// Executes the test case.
    ///
    /// This should not throw any exception: problems detected during execution
    /// are reported as a broken test case result.
    ///
    /// \param config The run-time configuration for the test case.
    /// \param hooks Run-time hooks to introspect the test case execution.
    /// \param stdout_path The file to which to redirect the stdout of the test.
    ///     If none, use a temporary file in the work directory.
    /// \param stderr_path The file to which to redirect the stdout of the test.
    ///     If none, use a temporary file in the work directory.
    ///
    /// \return The result of the execution.
    virtual test_result execute(
        const user_files::config& config, test_case_hooks& hooks,
        const utils::optional< utils::fs::path >& stdout_path,
        const utils::optional< utils::fs::path >& stderr_path) const = 0;

public:
    base_test_case(const base_test_program&, const std::string&);
    virtual ~base_test_case(void);

    const base_test_program& test_program(void) const;
    const std::string& name(void) const;

    properties_map all_properties(void) const;
    test_result debug(const user_files::config&,
                      test_case_hooks&,
                      const utils::fs::path&,
                      const utils::fs::path&) const;
    test_result run(const user_files::config&, test_case_hooks&) const;
};


/// Pointer to a test case.
typedef std::tr1::shared_ptr< base_test_case > test_case_ptr;


}  // namespace engine


#endif  // !defined(ENGINE_TEST_CASE_HPP)
