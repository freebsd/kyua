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

#include "model/test_program.hpp"

#include <algorithm>
#include <map>
#include <sstream>

#include "model/exceptions.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_result.hpp"
#include "utils/format/containers.ipp"
#include "utils/format/macros.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"
#include "utils/text/operations.ipp"

namespace fs = utils::fs;
namespace text = utils::text;

using utils::none;
using utils::optional;


namespace {


/// Predicate to compare two test cases via pointers to them.
///
/// \param tc1 Entry in a map of test case names to test case pointers.
/// \param tc2 Entry in a map of test case names to test case pointers.
///
/// \return True if the test case in tc1 is the same as in tc2.  Note that the
/// container test programs are NOT compared.
static bool
compare_test_case(const std::pair< std::string, model::test_case_ptr >& tc1,
                  const std::pair< std::string, model::test_case_ptr >& tc2)
{
    return tc1.first == tc2.first && *tc1.second == *tc2.second;
}


/// Compares if two sets of test cases hold the same values.
///
/// \param tests1 First collection of test cases.
/// \param tests2 Second collection of test cases.
///
/// \return True if both collections hold the same test cases (value-wise, not
/// pointer-wise); false otherwise.
static bool
compare_test_cases(const optional< model::test_cases_vector >& tests1,
                   const optional< model::test_cases_vector >& tests2)
{
    if (!tests1 && !tests2)
        return true;
    else if ((tests1 && !tests2) || (!tests1 && tests2))
        return false;
    INV(tests1 && tests2);

    // This is very inefficient, but because it should only be used in our own
    // tests, it doesn't matter.
    std::map< std::string, model::test_case_ptr > map1, map2;
    for (model::test_cases_vector::const_iterator iter = tests1.get().begin();
         iter != tests1.get().end(); ++iter)
        map1.insert(make_pair((*iter)->name(), *iter));
    for (model::test_cases_vector::const_iterator iter = tests2.get().begin();
         iter != tests2.get().end(); ++iter)
        map2.insert(make_pair((*iter)->name(), *iter));
    return std::equal(map1.begin(), map1.end(), map2.begin(),
                      compare_test_case);
}


}  // anonymous namespace


/// Internal implementation of a test_program.
struct model::test_program::impl {
    /// Name of the test program interface.
    std::string interface_name;

    /// Name of the test program binary relative to root.
    fs::path binary;

    /// Root of the test suite containing the test program.
    fs::path root;

    /// Name of the test suite this program belongs to.
    std::string test_suite_name;

    /// Metadata of the test program.
    model::metadata md;

    /// List of test cases in the test program; lazily initialized.
    optional< model::test_cases_vector > test_cases;

    /// Constructor.
    ///
    /// \param interface_name_ Name of the test program interface.
    /// \param binary_ The name of the test program binary relative to root_.
    /// \param root_ The root of the test suite containing the test program.
    /// \param test_suite_name_ The name of the test suite this program
    ///     belongs to.
    /// \param md_ Metadata of the test program.
    impl(const std::string& interface_name_, const fs::path& binary_,
         const fs::path& root_, const std::string& test_suite_name_,
         const model::metadata& md_) :
        interface_name(interface_name_),
        binary(binary_),
        root(root_),
        test_suite_name(test_suite_name_),
        md(md_)
    {
        PRE_MSG(!binary.is_absolute(),
                F("The program '%s' must be relative to the root of the test "
                  "suite '%s'") % binary % root);
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
                binary == other.binary &&
                root == other.root &&
                test_suite_name == other.test_suite_name &&
                md == other.md &&
                compare_test_cases(test_cases, other.test_cases));
    }
};


/// Constructs a new test program.
///
/// \param interface_name_ Name of the test program interface.
/// \param binary_ The name of the test program binary relative to root_.
/// \param root_ The root of the test suite containing the test program.
/// \param test_suite_name_ The name of the test suite this program belongs to.
/// \param md_ Metadata of the test program.
model::test_program::test_program(const std::string& interface_name_,
                                   const fs::path& binary_,
                                   const fs::path& root_,
                                   const std::string& test_suite_name_,
                                   const model::metadata& md_) :
    _pimpl(new impl(interface_name_, binary_, root_, test_suite_name_, md_))
{
}


/// Destroys a test program.
model::test_program::~test_program(void)
{
}


/// Gets the name of the test program interface.
///
/// \return An interface name.
const std::string&
model::test_program::interface_name(void) const
{
    return _pimpl->interface_name;
}


/// Gets the path to the test program relative to the root of the test suite.
///
/// \return The relative path to the test program binary.
const fs::path&
model::test_program::relative_path(void) const
{
    return _pimpl->binary;
}


/// Gets the absolute path to the test program.
///
/// \return The absolute path to the test program binary.
const fs::path
model::test_program::absolute_path(void) const
{
    const fs::path full_path = _pimpl->root / _pimpl->binary;
    return full_path.is_absolute() ? full_path : full_path.to_absolute();
}


/// Gets the root of the test suite containing this test program.
///
/// \return The path to the root of the test suite.
const fs::path&
model::test_program::root(void) const
{
    return _pimpl->root;
}


/// Gets the name of the test suite containing this test program.
///
/// \return The name of the test suite.
const std::string&
model::test_program::test_suite_name(void) const
{
    return _pimpl->test_suite_name;
}


/// Gets the metadata of the test program.
///
/// \return The metadata.
const model::metadata&
model::test_program::get_metadata(void) const
{
    return _pimpl->md;
}


/// Gets a test case by its name.
///
/// \param name The name of the test case to locate.
///
/// \return The requested test case.
///
/// \throw not_found_error If the specified test case is not in the test
///     program.
const model::test_case_ptr&
model::test_program::find(const std::string& name) const
{
    // TODO(jmmv): Should use a test_cases_map instead of a vector to optimize
    // lookups.
    if (has_test_cases()) {
        const model::test_cases_vector& tcs = test_cases();
        for (model::test_cases_vector::const_iterator iter = tcs.begin();
             iter != tcs.end(); iter++) {
            if ((*iter)->name() == name)
                return *iter;
        }
    }
    throw not_found_error(F("Unknown test case %s in test program %s") % name %
                          relative_path());
}


/// Gets the list of test cases from the test program.
///
/// \pre The list must have been set before with set_test_cases().
///
/// \return The list of test cases provided by the test program.
const model::test_cases_vector&
model::test_program::test_cases(void) const
{
    PRE(_pimpl->test_cases);
    return _pimpl->test_cases.get();
}


/// Checks to see if the test cases have been yet set.
///
/// \return True if set_test_cases() has been invoked.
bool
model::test_program::has_test_cases(void) const
{
    return _pimpl->test_cases;
}


/// Sets the collection of test cases included in this test program.
///
/// This function is provided so that when we load test programs from the
/// database we can populate them with the test cases they include.  We don't
/// want such test programs to be executed to gather this information.
///
/// We cannot provide this collection of tests in the constructor of the test
/// program because the test cases have to point to their test programs.
///
/// \pre The test program must not have attempted to load its test cases yet.
///     I.e. test_cases() has not been called.
///
/// \param test_cases_ The test cases to add to this test program.
void
model::test_program::set_test_cases(
    const model::test_cases_vector& test_cases_)
{
    PRE(!_pimpl->test_cases);
    _pimpl->test_cases = test_cases_;
}


/// Equality comparator.
///
/// \param other The other object to compare this one to.
///
/// \return True if this object and other are equal; false otherwise.
bool
model::test_program::operator==(const test_program& other) const
{
    return _pimpl == other._pimpl || *_pimpl == *other._pimpl;
}


/// Inequality comparator.
///
/// \param other The other object to compare this one to.
///
/// \return True if this object and other are different; false otherwise.
bool
model::test_program::operator!=(const test_program& other) const
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
model::operator<<(std::ostream& output, const test_program& object)
{
    output << F("test_program{interface=%s, binary=%s, root=%s, test_suite=%s, "
                "metadata=%s, test_cases=%s}")
        % text::quote(object.interface_name(), '\'')
        % text::quote(object.relative_path().str(), '\'')
        % text::quote(object.root().str(), '\'')
        % text::quote(object.test_suite_name(), '\'')
        % object.get_metadata()
        % object.test_cases();
    return output;
}
