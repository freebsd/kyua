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

/// \file engine/isolation.hpp
/// Utilities to implement test case execution routines.
///
/// This module provides a set of auxiliary functions to implement the execution
/// of test program binaries in a controlled manner.  This includes functions to
/// isolate the subprocess from the rest of the system, and functions to control
/// the proper cleanup of such subprocess when the parent process is interrupted.

#if !defined(ENGINE_ISOLATION_HPP)
#define ENGINE_ISOLATION_HPP

#include "utils/datetime.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.hpp"
#include "utils/process/status.hpp"

namespace engine {


namespace detail {


void interrupt_handler(const int);


}  // namespace detail


class test_result;


void check_interrupt(void);

utils::fs::path create_work_directory(void);

template< class Hook >
utils::optional< utils::process::status > fork_and_wait(
    Hook, const utils::fs::path&, const utils::fs::path&,
    const utils::datetime::delta&);

template< class Hook >
test_result protected_run(Hook);


}  // namespace engine

#endif  // !defined(ENGINE_ISOLATION_HPP)
