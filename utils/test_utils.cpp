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

extern "C" {
#include <regex.h>
#include <unistd.h>
}

#include <atf-c++.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "utils/fs/operations.hpp"
#include "utils/format/macros.hpp"
#include "utils/optional.ipp"
#include "utils/process/children.ipp"
#include "utils/test_utils.hpp"

namespace fs = utils::fs;
namespace process = utils::process;

using utils::optional;


utils::os_type utils::current_os =
#if defined(__FreeBSD__)
    utils::os_freebsd
#elif defined(__linux__)
    utils::os_linux
#elif defined(__NetBSD__)
    utils::os_netbsd
#elif defined(__SunOS__)
    utils::os_sunos
#else
    utils::os_unsupported
#endif
    ;


/// Dumps the contents of a file on the standard output.
///
/// \param prefix A string to use as a prefix for all the printed lines.  May be
///     empty.
/// \param path The path to the file to print.
void
utils::cat_file(const std::string& prefix, const fs::path& path)
{
    std::ifstream input(path.c_str());
    if (!input)
        ATF_FAIL(F("Cannot open file %s") % path);

    std::string line;
    while (std::getline(input, line).good()) {
        std::cout << prefix << line << "\n";
    }
}


/// Creates a file for testing.
///
/// Fails the test case if the file cannot be created.
///
/// \param file The name of the file to create.
/// \param contents The contents of the file, if any.  If empty, this writes
///     some arbitrary text into the file.
void
utils::create_file(const fs::path& file, const std::string& contents)
{
    std::ofstream output(file.c_str());
    if (!output)
        ATF_FAIL(F("Failed to create test file %s") % file);
    if (contents.empty())
        output << "Some contents\n";
    else
        output << contents;
}


/// Looks for a regular expression in a file.
///
/// \param regexp The regular expression.
/// \param path The path to the file to query.
///
/// \return True if the regular expression matches anywhere in the file; false
/// otherwise.
bool
utils::grep_file(const std::string& regexp, const fs::path& path)
{
    std::ifstream input(path.c_str());
    if (!input)
        ATF_FAIL(F("Cannot open file %s") % path);

    std::string line;
    while (std::getline(input, line).good()) {
        if (grep_string(regexp, line))
            return true;
    }
    return false;
}


/// Looks for a regular expression in a string.
///
/// \param regexp The regular expression.
/// \param path The string to query.
///
/// \return True if the regular expression matches anywhere in the string; false
/// otherwise.
bool
utils::grep_string(const std::string& regexp, const std::string& str)
{
    regex_t preg;
    ATF_REQUIRE(::regcomp(&preg, regexp.c_str(), REG_EXTENDED) == 0);
    const int res = ::regexec(&preg, str.c_str(), 0, NULL, 0);
    ATF_REQUIRE(res == 0 || res == REG_NOMATCH);
    ::regfree(&preg);
    return res == 0;
}


/// Looks for a regular expression in a vector of strings.
///
/// \param regexp The regular expression.
/// \param path The vector to query.
///
/// \return True if the regular expression matches anywhere in the vector; false
/// otherwise.
bool
utils::grep_vector(const std::string& regexp,
                   const std::vector< std::string >& v)
{
    for (std::vector< std::string >::const_iterator iter = v.begin();
         iter != v.end(); iter++) {
        if (grep_string(regexp, *iter))
            return true;
    }
    return false;
}


namespace {


/// Functor to execute 'mount -t tmpfs' (or a similar variant) in a subprocess.
class run_mount_tmpfs {
    optional< fs::path > _mount_binary;
    std::vector< std::string > _mount_args;

public:
    /// Constructor for the functor.
    ///
    /// \param mount_point The mount point location.  Must be absolute.
    run_mount_tmpfs(const fs::path& mount_point)
    {
        // Required for compatibility with, at least, SunOS.
        PRE(mount_point.is_absolute());

        switch (utils::current_os) {
        case utils::os_freebsd:
            _mount_binary = fs::find_in_path("mdmfs");
            _mount_args.push_back("-s");
            _mount_args.push_back("16m");
            _mount_args.push_back("md");
            _mount_args.push_back(mount_point.str());
            break;

        case utils::os_linux:
            _mount_binary = fs::find_in_path("mount");
            _mount_args.push_back("-t");
            _mount_args.push_back("tmpfs");
            _mount_args.push_back("tmpfs");
            _mount_args.push_back(mount_point.str());
            break;

        case utils::os_netbsd:
            _mount_binary = fs::find_in_path("mount");
            _mount_args.push_back("-t");
            _mount_args.push_back("tmpfs");
            _mount_args.push_back("tmpfs");
            _mount_args.push_back(mount_point.str());
            break;

        case utils::os_sunos:
            _mount_binary = fs::find_in_path("mount");
            _mount_args.push_back("-F");
            _mount_args.push_back("tmpfs");
            _mount_args.push_back("tmpfs");
            _mount_args.push_back(mount_point.str());
            break;

        default:
            ATF_SKIP("Don't know how to mount a file system for testing "
                     "purposes");
        }

        if (!_mount_binary)
            ATF_FAIL("Cannot locate tool '%s'; maybe sbin is not in the PATH?");
    }

    /// Does the actual mount.
    void
    operator()(void) const
    {
        PRE(_mount_binary);
        process::exec(_mount_binary.get(), _mount_args);
    }
};


}  // anonymous namespace


/// Mounts a temporary file system.
///
/// This is only provided for testing purposes.  The mounted file system
/// contains no valuable data.
///
/// Note that the calling test case is skipped if the current operating system
/// is not supported.
///
/// \param mount_point The path on which the file system will be mounted.
void
utils::mount_tmpfs(const fs::path& mount_point)
{
    // SunOS's mount(8) requires paths to be absolute.  To err on the side of
    // caution, let's make it absolute in all cases.
    const fs::path abs_mount_point = mount_point.is_absolute() ?
        mount_point : mount_point.to_absolute();

    const fs::path mount_out("mount.out");
    const fs::path mount_err("mount.err");

    std::auto_ptr< process::child_with_files > child =
        process::child_with_files::fork(run_mount_tmpfs(abs_mount_point),
                                        mount_out, mount_err);
    const process::status status = child->wait();
    cat_file("mount stdout: ", mount_out);
    cat_file("mount stderr: ", mount_err);
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(EXIT_SUCCESS, status.exitstatus());
}
