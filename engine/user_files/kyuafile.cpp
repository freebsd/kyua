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

#include <stdexcept>

#include "engine/user_files/common.hpp"
#include "engine/user_files/exceptions.hpp"
#include "engine/user_files/kyuafile.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/lua/exceptions.hpp"
#include "utils/lua/operations.hpp"
#include "utils/lua/wrap.ipp"
#include "utils/sanity.hpp"

namespace fs = utils::fs;
namespace lua = utils::lua;
namespace user_files = engine::user_files;


// These namespace blocks are here to help Doxygen match the functions to their
// prototypes...
namespace engine {
namespace user_files {
namespace detail {


/// Makes a relative path point inside a test suite.
///
/// \param in_path The input path to adjust.
/// \param root The directory where the initial Kyuafile is located.
///
/// \return The adjusted path.
fs::path
adjust_binary_path(const fs::path& in_path, const fs::path& root)
{
    if (in_path.is_absolute())
        return in_path;
    else {
        if (root.str() == ".")
            return in_path;
        else
            return root / in_path;
    }
}


/// Gets the data of a test program from the Lua state.
///
/// \pre stack(-1) contains a table describing a test program.
///
/// \param state The Lua state.
/// \param root The directory where the initial Kyuafile is located.
///
/// throw std::runtime_error If there is any problem in the input data.
/// throw fs::error If there is an invalid path in the input data.
test_program
get_test_program(lua::state& state, const fs::path& root)
{
    PRE(state.is_table());

    lua::stack_cleaner cleaner(state);

    state.push_string("name");
    state.get_table();
    if (!state.is_string())
        throw std::runtime_error("Found non-string name for test program");
    const fs::path path = adjust_binary_path(fs::path(state.to_string()),
                                             root);
    state.pop(1);

    state.push_string("test_suite");
    state.get_table();
    if (!state.is_string())
        throw std::runtime_error(F("Found non-string name for test suite of "
                                   "test program '%s'") % path);
    const std::string test_suite(state.to_string());
    state.pop(1);

    if (!fs::exists(path))
        throw std::runtime_error(F("Non-existent test program '%s'") % path);

    return test_program(path, test_suite);
}


/// Gets the data of a collection of test programs from the Lua state.
///
/// \param state The Lua state.
/// \param expr The expression that evaluates to the table with the test program
///     data.
/// \param root The directory where the initial Kyuafile is located.
///
/// throw std::runtime_error If there is any problem in the input data.
/// throw fs::error If there is an invalid path in the input data.
test_programs_vector
get_test_programs(lua::state& state, const std::string& expr,
                  const fs::path& root)
{
    lua::stack_cleaner cleaner(state);

    lua::eval(state, expr);
    if (!state.is_table())
        throw std::runtime_error(F("'%s' is not a table") % expr);

    test_programs_vector test_programs;

    state.push_nil();
    while (state.next()) {
        if (!state.is_table(-1))
            throw std::runtime_error(F("Expected table in '%s'") % expr);

        test_programs.push_back(get_test_program(state, root));

        state.pop(1);
    }

    return test_programs;
}


}  // namespace detail
}  // namespace user_files
}  // namespace engine


/// Initializes a test_program.
///
/// \param binary_path_ The path to the test program; can be either absolute or
///     relative.
/// \param test_suite_name_ The name of the test suite this test program belongs
///     to.
user_files::test_program::test_program(const utils::fs::path& binary_path_,
                                       const std::string& test_suite_name_) :
    binary_path(binary_path_),
    test_suite_name(test_suite_name_)
{
}


/// Constructs a kyuafile form initialized data.
///
/// Use load() to parse a test suite configuration file and construct a
/// kyuafile object.
///
/// \param tps_ Collection of test programs that belong to this test suite.
user_files::kyuafile::kyuafile(const test_programs_vector& tps_) :
    _test_programs(tps_)
{
}


/// Parses a test suite configuration file.
///
/// \param file The file to parse.
///
/// \return High-level representation of the configuration file.
///
/// \throw load_error If there is any problem loading the file.  This includes
///     file access errors and syntax errors.
user_files::kyuafile
user_files::kyuafile::load(const utils::fs::path& file)
{
    test_programs_vector test_programs;
    try {
        lua::state state;
        lua::stack_cleaner cleaner(state);

        const user_files::syntax_def syntax = user_files::do_user_file(
            state, file);
        if (syntax.first != "kyuafile")
            throw std::runtime_error(F("Unexpected file format '%s'; "
                                       "need 'kyuafile'") % syntax.first);
        if (syntax.second != 1)
            throw std::runtime_error(F("Unexpected file version '%d'; "
                                       "only 1 is supported") % syntax.second);

        test_programs = detail::get_test_programs(state,
                                                  "kyuafile.TEST_PROGRAMS",
                                                  file.branch_path());
    } catch (const std::runtime_error& e) {
        throw load_error(file, e.what());
    }
    return kyuafile(test_programs);
}


/// Gets the collection of test programs that belong to this test suite.
///
/// \return Collection of test program executable names.
const user_files::test_programs_vector&
user_files::kyuafile::test_programs(void) const
{
    return _test_programs;
}
