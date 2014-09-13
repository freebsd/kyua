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

#include "model/test_case.hpp"

#include "model/metadata.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "utils/format/macros.hpp"
#include "utils/optional.ipp"
#include "utils/text/operations.ipp"

namespace text = utils::text;

using utils::none;
using utils::optional;


/// Internal implementation for a test_case.
struct model::test_case::impl {
    /// Name of the interface implemented by the test program.
    const std::string interface_name;

    /// Test program this test case belongs to.
    const model::test_program& _test_program;

    /// Name of the test case; must be unique within the test program.
    std::string name;

    /// Test case metadata.
    model::metadata md;

    /// Fake result to return instead of running the test case.
    optional< model::test_result > fake_result;

    /// Constructor.
    ///
    /// \param interface_name_ Name of the interface implemented by the test
    ///     program.
    /// \param test_program_ The test program this test case belongs to.
    /// \param name_ The name of the test case within the test program.
    /// \param md_ Metadata of the test case.
    /// \param fake_result_ Fake result to return instead of running the test
    ///     case.
    impl(const std::string& interface_name_,
         const model::test_program& test_program_,
         const std::string& name_,
         const model::metadata& md_,
         const optional< model::test_result >& fake_result_) :
        interface_name(interface_name_),
        _test_program(test_program_),
        name(name_),
        md(md_),
        fake_result(fake_result_)
    {
    }

    /// Equality comparator.
    ///
    /// \param other The other object to compare this one to.
    ///
    /// \return True if this object and other are equal; false otherwise.
    bool
    operator==(const impl& other) const
    {
        return (interface_name == other.interface_name &&
                (_test_program.absolute_path() ==
                 other._test_program.absolute_path()) &&
                name == other.name &&
                md == other.md &&
                fake_result == other.fake_result);
    }
};


/// Constructs a new test case.
///
/// \param interface_name_ Name of the interface implemented by the test
///     program.
/// \param test_program_ The test program this test case belongs to.  This is a
///     static reference (instead of a test_program_ptr) because the test
///     program must exist in order for the test case to exist.
/// \param name_ The name of the test case within the test program.  Must be
///     unique.
/// \param md_ Metadata of the test case.
model::test_case::test_case(const std::string& interface_name_,
                             const model::test_program& test_program_,
                             const std::string& name_,
                             const model::metadata& md_) :
    _pimpl(new impl(interface_name_, test_program_, name_, md_, none))
{
}



/// Constructs a new fake test case.
///
/// A fake test case is a test case that is not really defined by the test
/// program.  Such test cases have a name surrounded by '__' and, when executed,
/// they return a fixed, pre-recorded result.
///
/// This is necessary for the cases where listing the test cases of a test
/// program fails.  In this scenario, we generate a single test case within
/// the test program that unconditionally returns a failure.
///
/// TODO(jmmv): Need to get rid of this.  We should be able to report the
/// status of test programs independently of test cases, as some interfaces
/// don't know about the latter at all.
///
/// \param interface_name_ Name of the interface implemented by the test
///     program.
/// \param test_program_ The test program this test case belongs to.
/// \param name_ The name to give to this fake test case.  This name has to be
///     prefixed and suffixed by '__' to clearly denote that this is internal.
/// \param description_ The description of the test case, if any.
/// \param test_result_ The fake result to return when this test case is run.
model::test_case::test_case(
    const std::string& interface_name_,
    const model::test_program& test_program_,
    const std::string& name_,
    const std::string& description_,
    const model::test_result& test_result_) :
    _pimpl(new impl(
        interface_name_, test_program_, name_,
        model::metadata_builder().set_description(description_).build(),
        utils::make_optional(test_result_)))
{
    PRE_MSG(name_.length() > 4 && name_.substr(0, 2) == "__" &&
            name_.substr(name_.length() - 2) == "__",
            "Invalid fake name provided to fake test case");
}


/// Destroys a test case.
model::test_case::~test_case(void)
{
}


/// Gets the name of the interface implemented by the test program.
///
/// \return An interface name.
const std::string&
model::test_case::interface_name(void) const
{
    return _pimpl->interface_name;
}


/// Gets the test program this test case belongs to.
///
/// \return A reference to the container test program.
const model::test_program&
model::test_case::container_test_program(void) const
{
    return _pimpl->_test_program;
}


/// Gets the test case name.
///
/// \return The test case name, relative to the test program.
const std::string&
model::test_case::name(void) const
{
    return _pimpl->name;
}


/// Gets the test case metadata.
///
/// \return The test case metadata.
const model::metadata&
model::test_case::get_metadata(void) const
{
    return _pimpl->md;
}


/// Gets the fake result pre-stored for this test case.
///
/// \return A fake result, or none if not defined.
optional< model::test_result >
model::test_case::fake_result(void) const
{
    return _pimpl->fake_result;
}


/// Equality comparator.
///
/// \warning Because test cases reference their container test programs, and
/// test programs include test cases, we cannot perform a full comparison here:
/// otherwise, we'd enter an inifinte loop.  Therefore, out of necessity, this
/// does NOT compare whether the container test programs of the affected test
/// cases are the same.
///
/// \param other The other object to compare this one to.
///
/// \return True if this object and other are equal; false otherwise.
bool
model::test_case::operator==(const test_case& other) const
{
    return _pimpl == other._pimpl || *_pimpl == *other._pimpl;
}


/// Inequality comparator.
///
/// \param other The other object to compare this one to.
///
/// \return True if this object and other are different; false otherwise.
bool
model::test_case::operator!=(const test_case& other) const
{
    return !(*this == other);
}


/// Injects the object into a stream.
///
/// \param output The stream into which to inject the object.
/// \param object The object to format.
///
/// \return The output stream.
std::ostream&
model::operator<<(std::ostream& output, const test_case& object)
{
    // We skip injecting container_test_program() on purpose to avoid a loop.
    output << F("test_case{interface=%s, name=%s, metadata=%s}")
        % text::quote(object.interface_name(), '\'')
        % text::quote(object.name(), '\'')
        % object.get_metadata();
    return output;
}
