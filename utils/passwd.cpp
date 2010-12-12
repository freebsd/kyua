// Copyright 2010, Google Inc.
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

extern "C" {
#include <unistd.h>
}

#include "utils/optional.ipp"
#include "utils/passwd.hpp"

namespace passwd = utils::passwd;


namespace {


/// If defined, replaces the value returned by current_user().
static utils::optional< passwd::user > fake_current_user;


}  // anonymous namespace


/// Constructs a new user.
///
/// \param uid_ The user identifier.
passwd::user::user(const int uid_) :
    _uid(uid_)
{
}


/// Checks if the user has superpowers or not.
///
/// \return True if the user is root, false otherwise.
bool
passwd::user::is_root(void) const
{
    return _uid == 0;
}


/// Returns the native user identifier.
///
/// \return The user identifier.
int
passwd::user::uid(void) const
{
    return _uid;
}


/// Gets the current user.
///
/// \return The current user.
passwd::user
passwd::current_user(void)
{
    if (fake_current_user)
        return fake_current_user.get();
    else
        return user(::getpid());
}


/// Overrides the current user for testing purposes.
///
/// This DOES NOT change the current privileges!
///
/// \param The new current user.
void
passwd::set_current_user_for_testing(const user& new_current_user)
{
    fake_current_user = new_current_user;
}
