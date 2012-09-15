// Copyright 2011 Google Inc.
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

#include "engine/plain_iface/test_program.hpp"

#include "engine/plain_iface/test_case.hpp"
#include "utils/optional.ipp"

namespace datetime = utils::datetime;
namespace plain_iface = engine::plain_iface;

using utils::optional;


datetime::delta engine::plain_iface::detail::default_timeout(300, 0);


/// Internal implementation for a test_program.
struct engine::plain_iface::test_program::impl {
    /// The timeout for the single test case in the test program.
    datetime::delta timeout;

    /// Constructor.
    ///
    /// \param optional_timeout_ The timeout for the test program's only single
    ///     test case.  If none, a default timeout is used.
    impl(const optional< datetime::delta >& optional_timeout_) :
        timeout(optional_timeout_ ?
                optional_timeout_.get() : detail::default_timeout)

    {
    }
};


/// Constructs a new plain test program.
///
/// \param binary_ The name of the test program binary relative to root_.
/// \param root_ The root of the test suite containing the test program.
/// \param test_suite_name_ The name of the test suite this program belongs to.
/// \param optional_timeout_ The timeout for the test program's only single test
///     case.  If none, a default timeout is used.
///
plain_iface::test_program::test_program(
    const utils::fs::path& binary_,
    const utils::fs::path& root_,
    const std::string& test_suite_name_,
    const optional< datetime::delta >& optional_timeout_) :
    base_test_program("plain", binary_, root_, test_suite_name_),
    _pimpl(new impl(optional_timeout_))
{
}


/// Destructor.
plain_iface::test_program::~test_program(void)
{
}


/// Returns the timeout of the test program.
///
/// Note that this is always defined, even in those cases where the test program
/// is constructed with a 'none' timeout.
///
/// \return The timeout value.
const datetime::delta&
plain_iface::test_program::timeout(void) const
{
    return _pimpl->timeout;
}


/// Loads the list of test cases contained in a test program.
///
/// \param test_program Test program from which to load the test cases.
///
/// \return A single test_case object representing the whole test program.
engine::test_cases_vector
plain_iface::load_plain_test_cases(const base_test_program* test_program)
{
    test_cases_vector loaded_test_cases;
    loaded_test_cases.push_back(engine::test_case_ptr(
                                    new test_case(*test_program)));
    return loaded_test_cases;
}
