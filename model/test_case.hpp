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

/// \file model/test_case.hpp
/// Definition of the "test case" concept.

#if !defined(MODEL_TEST_CASE_HPP)
#define MODEL_TEST_CASE_HPP

#include "model/test_case_fwd.hpp"

#include <ostream>
#include <string>

#include "model/metadata_fwd.hpp"
#include "model/test_program_fwd.hpp"
#include "model/test_result_fwd.hpp"
#include "utils/optional.hpp"
#include "utils/shared_ptr.hpp"

namespace model {


/// Representation of a test case.
class test_case {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::shared_ptr< impl > _pimpl;

public:
    test_case(const std::string&, const model::test_program&,
              const std::string&, const metadata&);
    test_case(const std::string&, const model::test_program&,
              const std::string&, const std::string&,
              const test_result&);
    ~test_case(void);

    const std::string& interface_name(void) const;
    const model::test_program& container_test_program(void) const;
    const std::string& name(void) const;
    const metadata& get_metadata(void) const;
    utils::optional< test_result > fake_result(void) const;

    bool operator==(const test_case&) const;
    bool operator!=(const test_case&) const;
};


std::ostream& operator<<(std::ostream&, const test_case&);


}  // namespace model

#endif  // !defined(MODEL_TEST_CASE_HPP)
