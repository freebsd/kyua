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

#if defined(HAVE_CONFIG_H)
#   include "config.h"
#endif

extern "C" {
#include <sys/stat.h>

#include <dirent.h>
#include <unistd.h>
}

#include <cerrno>
#include <cstring>

#include "utils/auto_array.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/sanity.hpp"

namespace fs = utils::fs;


namespace {


/// Gets the name of a directory entry as a sane type.
///
/// \param dp The directory entry as returned by readdir(3).
///
/// \return The name of the entry.
static std::string
dirent_name(const struct dirent* dp)
{
    utils::auto_array< char > name(new char[dp->d_namlen + 1]);
    std::memcpy(name.get(), dp->d_name, dp->d_namlen);
    name[dp->d_namlen] = '\0';
    return std::string(name.get());
}


/// Helper function for the public fs::cleanup() routine.
///
/// This helper function preserves the name of the top-level directory beeing
/// cleaned so that it can be reported in error messages.
///
/// TODO(jmmv): This should be careful not to cross mount points.  Not sure if
/// this should also take care of attempting to unmount such mount points.
/// (Consider a test case doing full integration testing of a file system and it
/// fails to unmount such file system from within its work directory).
///
/// \param root The directory where the cleanup traversal starts.  Must not
///     change in any recursive call.
/// \param current The directory being cleaned.
///
/// \throw fs::error If there is a problem removing any directory or file.
static void
cleanup_aux(const fs::path& root, const fs::path& current)
{
    if (::chmod(current.c_str(), 0700) == -1) {
        // We attempt to unprotect the directory to allow modifications, but if
        // this fails, we cannot do much more.  Just ignore the error and hope
        // that the removal of the directory and the files works later.

        // TODO(jmmv): Log this error.
    }

    DIR* dirp = ::opendir(current.c_str());
    if (dirp == NULL)
        throw fs::error(F("Failed to open directory %s while cleaning up %s") %
                        current % root);
    try {
        struct dirent* dp;
        while ((dp = ::readdir(dirp)) != NULL) {
            const std::string name = dirent_name(dp);
            if (name == "." || name == "..")
                continue;

            const fs::path entry_path = current / name;
            if (dp->d_type == DT_DIR) {
                cleanup_aux(root, entry_path);
            } else {
                if (::unlink(entry_path.c_str()) == -1) {
                    const int original_errno = errno;
                    throw fs::system_error(F("Failed to remove file %s while "
                                             "cleaning up %s") %
                                           entry_path % root, original_errno);
                }
            }
        }
    } catch (...) {
        ::closedir(dirp);
        throw;
    }
    ::closedir(dirp);

    if (::rmdir(current.c_str()) == -1) {
        const int original_errno = errno;
        throw fs::system_error(F("Failed to remove directory %s "
                                 "while cleaning up %s") %
                               current % root, original_errno);
    }
}


}  // anonymous namespace


/// Recursively removes a directory.
///
/// \param root The directory to remove.
///
/// \throw fs::error If there is a problem removing any directory or file.
void
fs::cleanup(const fs::path& root)
{
    cleanup_aux(root, root);
}


/// Queries the path to the current directory.
///
/// \return The path to the current directory.
///
/// \throw fs::error If there is a problem querying the current directory.
fs::path
fs::current_path(void)
{
    char* cwd;
#if defined(HAVE_GETCWD_DYN)
    cwd = ::getcwd(NULL, 0);
#else
    cwd = ::getcwd(NULL, MAXPATHLEN);
#endif
    if (cwd == NULL) {
        const int original_errno = errno;
        throw fs::system_error(F("Failed to get current working directory"),
                               original_errno);
    }

    try {
        const fs::path result(cwd);
        ::free(cwd);
        return result;
    } catch (...) {
        ::free(cwd);
        throw;
    }
}


/// Creates a directory.
///
/// \param dir The path to the directory to create.
/// \param mode The permissions for the new directory.
///
/// \throw system_error If the call to mkdir(2) fails.
void
fs::mkdir(const fs::path& dir, const int mode)
{
    if (::mkdir(dir.c_str(), static_cast< mode_t >(mode)) == -1) {
        const int original_errno = errno;
        throw fs::system_error(F("Failed to create directory %s") % dir,
                               original_errno);
    }
}


/// Creates a directory and any missing parents.
///
/// This is separate from the fs::mkdir function to clearly differentiate the
/// libc wrapper from the more complex algorithm implemented here.
///
/// \param dir The path to the directory to create.
/// \param mode The permissions for the new directories.
///
/// \throw system_error If any call to mkdir(2) fails.
void
fs::mkdir_p(const fs::path& dir, const int mode)
{
    try {
        fs::mkdir(dir, mode);
    } catch (const fs::system_error& e) {
        if (e.original_errno() == ENOENT) {
            fs::mkdir_p(dir.branch_path(), mode);
            fs::mkdir(dir, mode);
        } else
            throw e;
    }
}


/// Creates a temporary directory.
///
/// The temporary directory is created using mkstemp(3) using the provided
/// template.  This should be most likely used in conjunction with
/// fs::auto_directory.
///
/// \param path_template The template for the temporary path.  Must contain the
///     XXXXXX pattern, which is atomically replaced by a random unique string.
///
/// \return The generated path for the temporary directory.
///
/// \throw fs::system_error If the call to mkstemp(3) fails.
fs::path
fs::mkdtemp(const path& path_template)
{
    PRE(path_template.str().find("XXXXXX") != std::string::npos);
    utils::auto_array< char > buf(new char[path_template.str().length() + 1]);
    std::strcpy(buf.get(), path_template.c_str());
    if (::mkdtemp(buf.get()) == NULL) {
        const int original_errno = errno;
        throw fs::system_error(F("Cannot create temporary directory using "
                                 "template %s") % path_template,
                               original_errno);
    }
    return fs::path(buf.get());
}
