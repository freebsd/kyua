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

#include "engine/user_files/kyuafile.hpp"

#include <stdexcept>

#include <lutok/exceptions.hpp>
#include <lutok/operations.hpp>
#include <lutok/stack_cleaner.hpp>
#include <lutok/state.ipp>

#include "engine/test_program.hpp"
#include "engine/testers.hpp"
#include "engine/user_files/common.hpp"
#include "engine/user_files/exceptions.hpp"
#include "utils/datetime.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace user_files = engine::user_files;

using utils::none;
using utils::optional;


namespace {


/// Gets a string field from a Lua table.
///
/// \pre state(-1) contains a table.
///
/// \param state The Lua state.
/// \param field The name of the field to query.
/// \param error The error message to raise when an error condition is
///     encoutered.
///
/// \return The string value from the table.
///
/// \throw std::runtime_error If there is any problem accessing the table.
static inline std::string
get_table_string(lutok::state& state, const char* field,
                 const std::string& error)
{
    PRE(state.is_table());

    lutok::stack_cleaner cleaner(state);

    state.push_string(field);
    state.get_table();
    if (!state.is_string())
        throw std::runtime_error(error);
    const std::string str(state.to_string());
    state.pop(1);
    return str;
}


/// Gets a test program path name from a Lua test program definition.
///
/// \pre state(-1) contains a table representing a test program.
///
/// \param state The Lua state.
/// \param build_root The root location of the test suite.
///
/// \return The path to the test program relative to root.
///
/// \throw std::runtime_error If the table definition is invalid or if the test
///     program does not exist.
static fs::path
get_path(lutok::state& state, const fs::path& build_root)
{
    const fs::path path = fs::path(get_table_string(
        state, "name", "Found non-string name for test program"));
    if (path.is_absolute())
        throw std::runtime_error(F("Got unexpected absolute path for test "
                                   "program '%s'") % path);

    if (!fs::exists(build_root / path))
        throw std::runtime_error(F("Non-existent test program '%s'") % path);

    return path;
}


/// Gets a test suite name from a Lua test program definition.
///
/// \pre state(-1) contains a table representing a test program.
///
/// \param state The Lua state.
/// \param path The path to the test program; used for error reporting purposes.
///
/// \return The name of the test suite the test program belongs to.
///
/// \throw std::runtime_error If the table definition is invalid.
static std::string
get_test_suite(lutok::state& state, const fs::path& path)
{
    return get_table_string(
        state, "test_suite", F("Found non-string name for test suite of "
                               "test program '%s'") % path);
}


}  // anonymous namespace


// These namespace blocks are here to help Doxygen match the functions to their
// prototypes...
namespace engine {
namespace user_files {
namespace detail {


/// Gets the data of a test program from the Lua state.
///
/// \pre stack(-1) contains a table describing a test program.
///
/// \param state The Lua state.
/// \param build_root The directory where the initial Kyuafile is located.
///
/// \return The test program definition.
///
/// \throw std::runtime_error If there is any problem in the input data.
/// \throw fs::error If there is an invalid path in the input data.
test_program_ptr
get_test_program(lutok::state& state, const fs::path& build_root)
{
    PRE(state.is_table());

    lutok::stack_cleaner cleaner(state);

    const std::string interface = get_table_string(
        state, "interface", "Missing test case interface");
    try {
        (void)engine::tester_path(interface);
    } catch (const engine::error& e) {
        throw std::runtime_error(F("Unsupported test interface '%s'") %
                                 interface);
    }

    const fs::path path = get_path(state, build_root);
    const std::string test_suite = get_test_suite(state, path);

    metadata_builder mdbuilder;

    // TODO(jmmv): The definition of a test program should allow overriding ALL
    // of the metadata properties, not just the timeout.  See Issue 57.
    {
        state.push_string("timeout");
        state.get_table();
        if (state.is_nil()) {
            // Nothing to do.
        } else if (state.is_number()) {
            mdbuilder.set_timeout(datetime::delta(state.to_integer(), 0));
        } else {
            throw std::runtime_error(F("Non-integer value provided as timeout "
                                       "for test program '%s'") % path);
        }
        state.pop(1);
    }

    return test_program_ptr(new test_program(
        interface, path, build_root, test_suite, mdbuilder.build()));
}


/// Gets the data of a collection of test programs from the Lua state.
///
/// \param state The Lua state.
/// \param expr The expression that evaluates to the table with the test program
///     data.
/// \param build_root The directory where the initial Kyuafile is located.
///
/// \return The definition of the test programs.
///
/// \throw std::runtime_error If there is any problem in the input data.
/// \throw fs::error If there is an invalid path in the input data.
test_programs_vector
get_test_programs(lutok::state& state, const std::string& expr,
                  const fs::path& build_root)
{
    lutok::stack_cleaner cleaner(state);

    lutok::eval(state, expr);
    if (!state.is_table())
        throw std::runtime_error(F("'%s' is not a table") % expr);

    test_programs_vector test_programs;

    state.push_nil();
    while (state.next()) {
        if (!state.is_table(-1))
            throw std::runtime_error(F("Expected table in '%s'") % expr);

        test_programs.push_back(get_test_program(state, build_root));

        state.pop(1);
    }

    return test_programs;
}


}  // namespace detail
}  // namespace user_files
}  // namespace engine


/// Constructs a kyuafile form initialized data.
///
/// Use load() to parse a test suite configuration file and construct a
/// kyuafile object.
///
/// \param source_root_ The root directory for the test suite represented by the
///     Kyuafile.  In other words, the directory containing the first Kyuafile
///     processed.
/// \param build_root_ The root directory for the test programs themselves.  In
///     general, this will be the same as source_root_.  If different, the
///     specified directory must follow the exact same layout of source_root_.
/// \param tps_ Collection of test programs that belong to this test suite.
user_files::kyuafile::kyuafile(const fs::path& source_root_,
                               const fs::path& build_root_,
                               const test_programs_vector& tps_) :
    _source_root(source_root_),
    _build_root(build_root_),
    _test_programs(tps_)
{
}


/// Parses a test suite configuration file.
///
/// \param file The file to parse.
/// \param user_build_root If not none, specifies a path to a directory
///     containing the test programs themselves.  The layout of the build root
///     must match the layout of the source root (which is just the directory
///     from which the Kyuafile is being read).
///
/// \return High-level representation of the configuration file.
///
/// \throw load_error If there is any problem loading the file.  This includes
///     file access errors and syntax errors.
user_files::kyuafile
user_files::kyuafile::load(const fs::path& file,
                           const optional< fs::path > user_build_root)
{
    const fs::path source_root_ = file.branch_path();
    const fs::path build_root_ = user_build_root ?
        user_build_root.get() : source_root_;

    test_programs_vector test_programs;
    try {
        lutok::state state;
        lutok::stack_cleaner cleaner(state);

        const user_files::syntax_def syntax = user_files::do_user_file(
            state, file);
        if (syntax.first != "kyuafile")
            throw std::runtime_error(F("Unexpected file format '%s'; "
                                       "need 'kyuafile'") % syntax.first);
        if (syntax.second != 1)
            throw std::runtime_error(F("Unexpected file version '%s'; "
                                       "only 1 is supported") % syntax.second);

        test_programs = detail::get_test_programs(state,
                                                  "kyuafile.TEST_PROGRAMS",
                                                  build_root_);
    } catch (const std::runtime_error& e) {
        throw load_error(file, e.what());
    }
    return kyuafile(source_root_, build_root_, test_programs);
}


/// Gets the root directory of the test suite.
///
/// \return A path.
const fs::path&
user_files::kyuafile::source_root(void) const
{
    return _source_root;
}


/// Gets the root directory of the test programs.
///
/// \return A path.
const fs::path&
user_files::kyuafile::build_root(void) const
{
    return _build_root;
}


/// Gets the collection of test programs that belong to this test suite.
///
/// \return Collection of test program executable names.
const engine::test_programs_vector&
user_files::kyuafile::test_programs(void) const
{
    return _test_programs;
}
