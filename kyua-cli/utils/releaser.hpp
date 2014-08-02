// Copyright 2014 Google Inc.
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

/// \file utils/releaser.hpp
/// Provides the utils::releaser class.

#if !defined(UTILS_RELEASER_HPP)
#define UTILS_RELEASER_HPP

namespace utils {


/// RAII pattern to invoke a release method on destruction.
///
/// \tparam Object The type of the object to be released.  Not a pointer.
/// \tparam ReturnType The return type of the release method.
template< typename Object, typename ReturnType >
class releaser {
    /// Pointer to the object being managed.
    Object* _object;

    /// Release hook.
    ReturnType (*_free_hook)(Object*);

public:
    /// Constructor.
    ///
    /// \param object Pointer to the object being managed.
    /// \param free_hook Release hook.
    releaser(Object* object, ReturnType (*free_hook)(Object*)) :
        _object(object), _free_hook(free_hook)
    {
    }

    /// Destructor.
    ~releaser(void)
    {
        _free_hook(_object);
    }
};


}  // namespace utils


#endif  // !defined(UTILS_RELEASER_HPP)
