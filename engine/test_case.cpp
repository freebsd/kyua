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

#include "engine/test_case.hpp"
#include "utils/sanity.hpp"

using engine::properties_map;
using engine::test_case;


/// Constructs a new test case.
///
/// \param program_ Name of the test program containing the test case.
/// \param name_ Name of the test case.  This name comes from its "ident"
///     meta-data property.
/// \param metadata_ Meta-data properties, not including "ident".
test_case::test_case(const utils::fs::path& program_, const std::string& name_,
                     const properties_map& metadata_) :
    _program(program_),
    _name(name_),
    _metadata(metadata_)
{
    PRE(_metadata.find("ident") == _metadata.end());
}


/// Gets the name of the test program containing the test case.
///
/// \return The test program executable name.
const utils::fs::path&
test_case::program(void) const
{
    return _program;
}


/// Gets the test case name.
///
/// \return The test case name.
const std::string&
test_case::name(void) const
{
    return _name;
}


/// Gets the meta-data properties of the test case.
///
/// \return The meta-data properties.
const properties_map&
test_case::metadata(void) const
{
    return _metadata;
}


/// Equality comparator.
///
/// \param tc The test case to compare this test case to.
///
/// \return bool True if the test cases are equal, false otherwise.
bool
test_case::operator==(const test_case& tc) const
{
    return _program == tc._program && _name == tc._name &&
        _metadata == tc._metadata;
}
