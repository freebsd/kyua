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

#if defined(HAVE_CONFIG_H)
#   include "config.h"
#endif

extern "C" {
#if defined(HAVE_UNMOUNT)
#   include <sys/param.h>
#   include <sys/mount.h>
#endif
#include <sys/stat.h>

#include <dirent.h>
#include <unistd.h>
}

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>

#include "utils/auto_array.ipp"
#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/optional.ipp"
#include "utils/process/children.ipp"
#include "utils/sanity.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace process = utils::process;

using utils::none;
using utils::optional;


/// Specifies if a real unmount(2) is available.
static const bool have_unmount2 =
#if defined(HAVE_UNMOUNT)
    true;
#else
    false;
#endif


#if !defined(UMOUNT)
/// Substitute value for the path to umount(8).
#   define UMOUNT "do-not-use-this-value"
#else
#   if defined(HAVE_UNMOUNT)
#       error "umount(8) detected when unmount(2) is also available"
#   endif
#endif


#if !defined(HAVE_UNMOUNT)
/// Fake unmount(2) function for systems without it.
///
/// This is only provided to allow our code to compile in all platforms
/// regardless of whether they actually have an unmount(2) or not.
static void
unmount(const char* unused_path, const int unused_flags)
{
    UNREACHABLE_MSG("Can't be called if have_unmount2 is false");
}
#endif


namespace {


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
        const int original_errno = errno;
        LW(F("Failed to chmod 0700 temporary directory '%s': %s") %
           current.str() % std::strerror(original_errno));
    }

    DIR* dirp = ::opendir(current.c_str());
    if (dirp == NULL)
        throw fs::error(F("Failed to open directory %s while cleaning up %s") %
                        current % root);
    try {
        struct dirent* dp;
        while ((dp = ::readdir(dirp)) != NULL) {
            const std::string name = dp->d_name;
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


/// Unmounts a file system using unmount(2).
///
/// \pre unmount(2) must be available; i.e. have_unmount2 must be true.
///
/// \param mount_point The file system to unmount.
///
/// \throw fs::system_error If there is a problem unmounting the file system.
static void
unmount_with_unmount2(const fs::path& mount_point)
{
    PRE(have_unmount2);

    static const int unmount_retries = 5;
    static const int unmount_retry_delay_seconds = 5;

    int retries = unmount_retries;

retry_unmount:
    if (::unmount(mount_point.c_str(), 0) == -1) {
        if (errno == EBUSY && retries > 0) {
            LD(F("Unmount failed; sleeping before retrying"));
            retries--;
            ::sleep(unmount_retry_delay_seconds);
            goto retry_unmount;
        } else {
            const int original_errno = errno;
            throw fs::system_error(F("Failed to unmount '%s'") %
                                   mount_point, original_errno);
        }
    }
}


/// Functor to execute umount(8).
class run_umount {
    fs::path _mount_point;

public:
    /// Constructs the functor.
    ///
    /// \param mount_point The file system to unmount.
    run_umount(const fs::path& mount_point) :
        _mount_point(mount_point)
    {
    }

    /// Executes umount(8) to unmount the file system.
    void
    operator()(void) {
        const fs::path umount_binary(UMOUNT);
        if (!umount_binary.is_absolute())
            LW(F("Builtin path '%s' to umount(8) is not absolute") %
               umount_binary.str());

        std::vector< std::string > args;
        args.push_back(umount_binary.str());
        args.push_back(_mount_point.str());
        process::exec(umount_binary, args);
    };
};


/// Unmounts a file system using umount(8).
///
/// \pre umount(2) must not be available; i.e. have_unmount2 must be false.
///
/// \param mount_point The file system to unmount.
///
/// \throw fs::error If there is a problem unmounting the file system.
static void
unmount_with_umount8(const fs::path& mount_point)
{
    PRE(!have_unmount2);

    static const datetime::delta timeout(30, 0);

    std::auto_ptr< process::child_with_output > child(
        process::child_with_output::fork(run_umount(mount_point)));

    try {
        std::string line;
        while (std::getline(child->output(), line).good()) {
            LI(F("umount(8) output: %s") % line);
        }
    } catch (...) {
        // Just ignore this.  It is not that important to capture the
        // messages, yet we want to ensure we wait for the child process.
        LD("Caught exception while processing umount(8) output");
    }

    const process::status status = child->wait(timeout);
    if (!status.exited() || status.exitstatus() != EXIT_SUCCESS)
        throw fs::error(F("umount(8) failed while unmounting '%s'") %
                        mount_point);
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
        std::free(cwd);
        return result;
    } catch (...) {
        std::free(cwd);
        throw;
    }
}


/// Checks if a file exists.
///
/// Be aware that this is racy in the same way as access(2) is.
///
/// \param path The file to check the existance of.
///
/// \return True if the file exists; false otherwise.
bool
fs::exists(const fs::path& path)
{
    return ::access(path.c_str(), F_OK) == 0;
}


/// Locates a file in the PATH.
///
/// \param name The file to locate.
///
/// \return The path to the located file or none if it was not found
optional< fs::path >
fs::find_in_path(const char* name)
{
    const optional< std::string > current_path = utils::getenv("PATH");
    if (!current_path || current_path.get().empty())
        return none;

    std::istringstream path_input(current_path.get() + ":");
    std::string path_component;
    while (std::getline(path_input, path_component, ':').good()) {
        const fs::path candidate = path_component.empty() ?
            fs::path(name) : (fs::path(path_component) / name);
        if (exists(candidate))
            return utils::make_optional(candidate);
    }
    return none;
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
        } else if (e.original_errno() != EEXIST)
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


/// Unmounts a file system.
///
/// \param mount_point The file system to unmount.
///
/// \throw fs::error If there is any problem unmounting the file system.
void
fs::unmount(const path& mount_point)
{
    // FreeBSD's unmount(2) requires paths to be absolute.  To err on the side
    // of caution, let's make it absolute in all cases.
    const path abs_mount_point = mount_point.is_absolute() ?
        mount_point : mount_point.to_absolute();

    if (have_unmount2) {
        LD(F("Unmounting %s using unmount(2)") % abs_mount_point);
        unmount_with_unmount2(abs_mount_point);
    } else {
        LD(F("Unmounting %s using umount(8)") % abs_mount_point);
        unmount_with_umount8(abs_mount_point);
    }
}
