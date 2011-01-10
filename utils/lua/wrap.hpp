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

/// \file utils/lua/wrap.hpp
/// Wrapper classes and utilities for the Lua C library.
///
/// This module contains thin RAII wrappers around the Lua structures and
/// lightweight, safer wrapper methods around the Lua C API.

#if !defined(UTILS_LUA_WRAP_HPP)
#define UTILS_LUA_WRAP_HPP

#include <memory>

#include "utils/fs/path.hpp"
#include "utils/noncopyable.hpp"

namespace utils {
namespace lua {


/// A RAII model for the Lua state.
///
/// This class holds the state of the Lua interpreter during its existence and
/// provides wrappers around several Lua library functions that operate on such
/// state.
///
/// These wrapper functions differ from the C versions in that they use the
/// implicit state hold by the class, they use C++ types where appropriate and
/// they use exceptions to report errors.
///
/// The wrappers intend to be as lightweight as possible but, in some
/// situations, they are pretty complex because they need to do extra work to
/// capture the errors reported by the Lua C API.  We prefer having fine-grained
/// error control rather than efficiency, so this is OK.
class state : utils::noncopyable {
    struct impl;
    std::auto_ptr< impl > _pimpl;

public:
    state(void);
    ~state(void);

    void close(void);
    void get_global(const std::string&);
    void get_table(const int = -2);
    int get_top(void);
    bool is_nil(const int = -1);
    bool is_number(const int = -1);
    bool is_string(const int = -1);
    bool is_table(const int = -1);
    void load_file(const utils::fs::path&);
    void load_string(const std::string&);
    void open_base(void);
    void open_string(void);
    void open_table(void);
    void pcall(const int, const int, const int);
    void pop(const int);
    void push_integer(const int);
    long to_integer(const int = -1);
    std::string to_string(const int = -1);

    void* raw_state_for_testing(void);
};


/// A RAII model for values on the Lua stack.
///
/// At creation time, the object records the current depth of the Lua stack and,
/// during destruction, restores the recorded depth by popping as many stack
/// entries as required.  As a corollary, the stack can only grow during the
/// lifetime of a stack_cleaner object (or shrink, but cannot become shorter
/// than the depth recorded at creation time).
///
/// Use this class as follows:
///
/// state s;
/// {
///     stack_cleaner cleaner1(s);
///     s.push_integer(3);
///     s.push_integer(5);
///     ... do stuff here ...
///     for (...) {
///         stack_cleaner cleaner2(s);
///         s.load_string("...");
///         s.pcall(0, 1, 0);
///         ... do stuff here ...
///     }
///     // cleaner2 destroyed; the result of pcall is gone.
/// }
/// // cleaner1 destroyed; the integers 3 and 5 are gone.
///
/// You must give a name to the instantiated objects even if they cannot be
/// accessed later.  Otherwise, the instance will be destroyed right away and
/// will not have the desired effect.
class stack_cleaner : noncopyable {
    struct impl;
    std::auto_ptr< impl > _pimpl;

public:
    stack_cleaner(state&);
    ~stack_cleaner(void);

    void forget(void);
};


}  // namespace lua
}  // namespace utils

#endif  // !defined(UTILS_LUA_WRAP_HPP)
