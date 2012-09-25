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

/// \file engine/atf_iface/test_case.hpp
/// Provides the atf-specific test_case class and other auxiliary types.

#if !defined(ENGINE_ATF_IFACE_TEST_CASE_HPP)
#define ENGINE_ATF_IFACE_TEST_CASE_HPP

#include <map>
#include <set>
#include <string>

#include "engine/metadata.hpp"
#include "engine/test_case.hpp"
#include "utils/datetime.hpp"
#include "utils/fs/path.hpp"
#include "utils/units.hpp"

namespace engine {
namespace atf_iface {


namespace detail {


bool parse_bool(const std::string&, const std::string&);
unsigned long parse_ulong(const std::string&, const std::string&);


}  // namespace detail


/// Representation of an ATF test case.
///
/// Test cases should be thought as free-standing entities: even though they
/// are located within a test program, the test program serves no other purpose
/// than to provide a way to execute the test cases.  Therefore, no information
/// needs to be stored for the test programs themselves.
class test_case : public base_test_case {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::tr1::shared_ptr< impl > _pimpl;

    properties_map get_all_properties(void) const;

public:
    test_case(const base_test_program&, const std::string&, const std::string&,
              const bool, const utils::datetime::delta&, const metadata&,
              const properties_map&);
    test_case(const base_test_program&, const std::string&, const std::string&,
              const test_result&);
    ~test_case(void);

    static test_case from_properties(const base_test_program&,
                                     const std::string&, const properties_map&);

    const std::string& description(void) const;
    bool has_cleanup(void) const;
    const utils::datetime::delta& timeout(void) const;
    const metadata& get_metadata(void) const;
    const strings_set& allowed_architectures(void) const;
    const strings_set& allowed_platforms(void) const;
    const strings_set& required_configs(void) const;
    const paths_set& required_files(void) const;
    const utils::units::bytes& required_memory(void) const;
    const paths_set& required_programs(void) const;
    const std::string& required_user(void) const;
    const properties_map& user_metadata(void) const;

    utils::optional< test_result > fake_result(void) const;

    std::string check_requirements(const utils::config::tree&) const;
};


test_result debug_atf_test_case(const base_test_case*,
                                const utils::config::tree&, test_case_hooks&,
                                const utils::fs::path&, const utils::fs::path&);
test_result run_atf_test_case(const base_test_case*, const utils::config::tree&,
                              test_case_hooks&);


}  // namespace atf_iface
}  // namespace engine


#endif  // !defined(ENGINE_ATF_IFACE_TEST_CASE_HPP)
