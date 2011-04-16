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

/// \file engine/user_files/exceptions.hpp
/// Exception types raised by the user_files module.

#if !defined(ENGINE_USER_FILES_EXCEPTIONS_HPP)
#define ENGINE_USER_FILES_EXCEPTIONS_HPP

#include "engine/exceptions.hpp"
#include "utils/fs/path.hpp"

namespace engine {
namespace user_files {


/// Base exception for engine errors.
class error : public engine::error {
public:
    explicit error(const std::string&);
    virtual ~error(void) throw();
};


/// Error while parsing external data.
class load_error : public error {
public:
    utils::fs::path file;
    std::string reason;

    explicit load_error(const utils::fs::path&, const std::string&);
    virtual ~load_error(void) throw();
};


}  // namespace user_files
}  // namespace engine


#endif  // !defined(ENGINE_USER_FILES_EXCEPTIONS_HPP)
