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

#include "engine/test_program.hpp"
#include "utils/format/macros.hpp"
#include "utils/sanity.hpp"

namespace fs = utils::fs;


/// Constructs a new test program.
///
/// \param binary_ The name of the test program binary relative to root_.
/// \param root_ The root of the test suite containing the test program.
/// \param test_suite_name_ The name of the test suite this program belongs to.
engine::test_program::test_program(const utils::fs::path& binary_,
                                   const utils::fs::path& root_,
                                   const std::string& test_suite_name_) :
    _binary(binary_),
    _root(root_),
    _test_suite_name(test_suite_name_)
{
    PRE_MSG(!_binary.is_absolute(), F("The program '%s' must be relative to "
        "the root of the test suite '%s'") % _binary % _root);
}


/// Destroys a test program.
engine::test_program::~test_program(void)
{
}


/// Gets the path to the test program relative to the root of the test suite.
///
/// \return The relative path to the test program binary.
const fs::path&
engine::test_program::relative_path(void) const
{
    return _binary;
}


/// Gets the absolute path to the test program.
///
/// \return The absolute path to the test program binary.
const fs::path
engine::test_program::absolute_path(void) const
{
    return _root / _binary;
}


/// Gets the root of the test suite containing this test program.
///
/// \return The path to the root of the test suite.
const fs::path&
engine::test_program::root(void) const
{
    return _root;
}


/// Gets the name of the test suite containing this test program.
///
/// \return The name of the test suite.
const std::string&
engine::test_program::test_suite_name(void) const
{
    return _test_suite_name;
}


/// Gets the list of test cases from the test program.
///
/// Note that this operation may be expensive and may throw arbitrary runtime
/// errors.
///
/// \return The list of test cases provided by the test program.
///
/// \throw engine::error If there are problems loading the test case list.
const engine::test_cases_vector&
engine::test_program::test_cases(void) const
{
    if (_test_cases.empty())
        _test_cases = load_test_cases();
    return _test_cases;
}
