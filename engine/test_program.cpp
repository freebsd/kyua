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

#include "engine/test_program.hpp"

#include <stdexcept>

#include <lutok/operations.hpp>
#include <lutok/state.ipp>

#include "engine/testers.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"
#include "utils/optional.ipp"

namespace fs = utils::fs;

using utils::none;


namespace {


/// Lua hook for the test_case function.
///
/// \pre state(-1) contains the arguments to the function.
///
/// \param state The Lua state in which we are running.
///
/// \return The number of return values, which is always 0.
static int
lua_test_case(lutok::state& state)
{
    if (!state.is_table(-1))
        throw std::runtime_error("Oh noes"); // XXX

    state.get_global("_test_cases");
    model::test_cases_vector* test_cases =
        *state.to_userdata< model::test_cases_vector* >(-1);
    state.pop(1);

    state.get_global("_test_program");
    const model::test_program* test_program =
        *state.to_userdata< model::test_program* >(-1);
    state.pop(1);

    state.push_string("name");
    state.get_table(-2);
    const std::string name = state.to_string(-1);
    state.pop(1);

    model::metadata_builder mdbuilder(test_program->get_metadata());

    state.push_nil();
    while (state.next(-2)) {
        if (!state.is_string(-2))
            throw std::runtime_error("Oh oh");  // XXX
        const std::string property = state.to_string(-2);

        if (!state.is_string(-1))
            throw std::runtime_error("Oh oh");  // XXX
        const std::string value = state.to_string(-1);

        if (property != "name")
            mdbuilder.set_string(property, value);

        state.pop(1);
    }
    state.pop(1);

    model::test_case_ptr test_case(
        new model::test_case(test_program->interface_name(), *test_program,
                             name, mdbuilder.build()));
    test_cases->push_back(test_case);

    return 0;
}


/// Sets up the Lua state to process the output of a test case list.
///
/// \param [in,out] state The Lua state to configure.
/// \param test_program Pointer to the test program being loaded.
/// \param [out] test_cases Vector that will contain the list of test cases.
static void
setup_lua_state(lutok::state& state,
                const model::test_program* test_program,
                model::test_cases_vector* test_cases)
{
    *state.new_userdata< model::test_cases_vector* >() = test_cases;
    state.set_global("_test_cases");

    *state.new_userdata< const model::test_program* >() = test_program;
    state.set_global("_test_program");

    state.push_cxx_function(lua_test_case);
    state.set_global("test_case");
}


/// Loads the list of test cases from a test program.
///
/// \param test_program Representation of the test program to load.
///
/// \return A list of test cases.
static model::test_cases_vector
load_test_cases(const model::test_program& test_program)
{
    const engine::tester tester(test_program.interface_name(), none, none);
    const std::string output = tester.list(test_program.absolute_path());

    model::test_cases_vector test_cases;
    lutok::state state;
    setup_lua_state(state, &test_program, &test_cases);
    lutok::do_string(state, output, 0, 0, 0);
    return test_cases;
}


}  // anonymous namespace


/// Gets the list of test cases from the test program.
///
/// \param [in,out] program The test program to update with the loaded list of
///     test cases.
void
engine::load_test_cases(model::test_program& program)
{
    if (!program.has_test_cases()) {
        model::test_cases_vector test_cases;
        try {
            test_cases = ::load_test_cases(program);
        } catch (const std::runtime_error& e) {
            // TODO(jmmv): This is a very ugly workaround for the fact that we
            // cannot report failures at the test-program level.  We should
            // either address this, or move this reporting to the testers
            // themselves.
            LW(F("Failed to load test cases list: %s") % e.what());
            model::test_cases_vector fake_test_cases;
            fake_test_cases.push_back(model::test_case_ptr(new model::test_case(
                program.interface_name(), program, "__test_cases_list__",
                "Represents the correct processing of the test cases list",
                model::test_result(model::test_result::broken, e.what()))));
            test_cases = fake_test_cases;
        }
        program.set_test_cases(test_cases);
    }
}
