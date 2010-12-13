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
#include <sys/types.h>

#include <pwd.h>
#include <unistd.h>
}

#include <stdexcept>

#include "utils/format/macros.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"
#include "utils/sanity.hpp"

namespace passwd_ns = utils::passwd;


namespace {


/// If defined, replaces the value returned by current_user().
static utils::optional< passwd_ns::user > fake_current_user;


}  // anonymous namespace


/// Constructs a new user.
///
/// \param name_ The name of the user.
/// \param uid_ The user identifier.
/// \param gid_ The login group identifier.
passwd_ns::user::user(const std::string& name_, const unsigned int uid_,
                      const unsigned int gid_) :
    name(name_),
    uid(uid_),
    gid(gid_)
{
}


/// Checks if the user has superpowers or not.
///
/// \return True if the user is root, false otherwise.
bool
passwd_ns::user::is_root(void) const
{
    return uid == 0;
}


/// Gets the current user.
///
/// \return The current user.
passwd_ns::user
passwd_ns::current_user(void)
{
    if (fake_current_user)
        return fake_current_user.get();
    else
        return find_user_by_uid(::getuid());
}


/// Drops privileges to the specified user.
///
/// \param unprivileged_user The user to drop privileges to.
///
/// \throw std::runtime_error If there is any problem dropping privileges.
void
passwd_ns::drop_privileges(const user& unprivileged_user)
{
    PRE(::getuid() == 0);

    if (::setgid(unprivileged_user.gid) == -1)
        throw std::runtime_error(F("Failed to drop group privileges (current "
                                   "GID %d, new GID %d)")
                                 % ::getgid() % unprivileged_user.gid);

    if (::setuid(unprivileged_user.uid) == -1)
        throw std::runtime_error(F("Failed to drop user privileges (current "
                                   "UID %d, new UID %d")
                                 % ::getuid() % unprivileged_user.uid);
}


/// Gets information about a user by its name.
///
/// \param name The name of the user to query.
///
/// \return The information about the user.
///
/// \throw std::runtime_error If the user does not exist.
passwd_ns::user
passwd_ns::find_user_by_name(const std::string& name)
{
    const struct ::passwd* pw = ::getpwnam(name.c_str());
    if (pw == NULL)
        throw std::runtime_error(F("Failed to get information about the user "
                                   "'%s'") % name);
    INV(pw->pw_name == name);
    return user(pw->pw_name, pw->pw_uid, pw->pw_gid);
}


/// Gets information about a user by its identifier.
///
/// \param name The identifier of the user to query.
///
/// \return The information about the user.
///
/// \throw std::runtime_error If the user does not exist.
passwd_ns::user
passwd_ns::find_user_by_uid(const unsigned int uid)
{
    const struct ::passwd* pw = ::getpwuid(uid);
    if (pw == NULL)
        throw std::runtime_error(F("Failed to get information about the user "
                                   "with UID %d") % uid);
    INV(pw->pw_uid == uid);
    return user(pw->pw_name, pw->pw_uid, pw->pw_gid);
}


/// Overrides the current user for testing purposes.
///
/// This DOES NOT change the current privileges!
///
/// \param user The new current user.
void
passwd_ns::set_current_user_for_testing(const user& new_current_user)
{
    fake_current_user = new_current_user;
}
