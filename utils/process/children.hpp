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

/// \file utils/process/children.hpp
/// Spawning and manipulation of children processes.
///
/// The children module provides a set of functions to spawn subprocesses with
/// different settings, and the corresponding set of classes to interact with
/// said subprocesses.  The interfaces to fork subprocesses are very simplified
/// and only provide the minimum functionality required by the rest of the
/// project.
///
/// Be aware that the semantics of the fork and wait methods exposed by this
/// module are slightly different from that of the native calls.  Any process
/// spawned by fork here will be isolated in its own process group; once any of
/// such children processes is awaited for, its whole process group will be
/// terminated.  This is the semantics we want in the above layers to ensure
/// that test programs (and, for that matter, external utilities) do not leak
/// subprocesses on the system.

#if !defined(UTILS_PROCESS_CHILDREN_HPP)
#define UTILS_PROCESS_CHILDREN_HPP

#include <istream>
#include <memory>
#include <string>
#include <vector>

#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/fs/path.hpp"
#include "utils/noncopyable.hpp"
#include "utils/process/status.hpp"

namespace utils {
namespace process {


/// Child process that writes stdout and stderr to files.
///
/// Use this class when you want to start a child process and you want to store
/// all of its output to stdout and stderr in separate files for later
/// processing.
class child_with_files : noncopyable {
    struct impl;
    std::auto_ptr< impl > _pimpl;

    static std::auto_ptr< child_with_files > fork_aux(
        const fs::path&, const fs::path&);

    explicit child_with_files(impl *);

public:
    ~child_with_files(void);

    template< typename Hook >
    static std::auto_ptr< child_with_files > fork(Hook, const fs::path&,
                                                  const fs::path&);

    status wait(const datetime::delta& = datetime::delta());
};


/// Child process that merges stdout and stderr and exposes them as an stream.
///
/// Use this class when you want to start a child process and you want to
/// process its output programmatically as it is generated.  The muxing of
/// stdout and stderr is performed at the subprocess level so that the caller
/// does not have to deal with poll(2).
class child_with_output : noncopyable {
    struct impl;
    std::auto_ptr< impl > _pimpl;

    explicit child_with_output(impl *);

    static std::auto_ptr< child_with_output > fork_aux(void);

public:
    ~child_with_output(void);

    template< typename Hook >
    static std::auto_ptr< child_with_output > fork(Hook);

    std::istream& output(void);

    status wait(const datetime::delta& = datetime::delta());
};


void exec(const fs::path&, const std::vector< std::string >&) UTILS_NORETURN;


}  // namespace process
}  // namespace utils

#endif  // !defined(UTILS_PROCESS_CHILDREN_HPP)
