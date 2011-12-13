// Copyright 2010 Google Inc.
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
#include <sys/stat.h>
#include <sys/wait.h>

#include <dirent.h>
#include <signal.h>
#include <unistd.h>
}

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

#include <atf-c++.hpp>

#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/test_utils.hpp"

namespace fs = utils::fs;


namespace {


/// Checks if a directory entry exists and matches a specific type.
///
/// \param dir The directory in which to look for the entry.
/// \param name The name of the entry to look up.
/// \param expected_type The expected type of the file as given by dir(5).
///
/// \return True if the entry exists and matches the given type; false
/// otherwise.
static bool
lookup(const char* dir, const char* name, const int expected_type)
{
    DIR* dirp = ::opendir(dir);
    ATF_REQUIRE(dirp != NULL);

    bool found = false;
    struct dirent* dp;
    while (!found && (dp = readdir(dirp)) != NULL) {
        if (std::strcmp(dp->d_name, name) == 0 &&
            dp->d_type == expected_type) {
            found = true;
        }
    }
    ::closedir(dirp);
    return found;
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(cleanup__file);
ATF_TEST_CASE_BODY(cleanup__file)
{
    utils::create_file(fs::path("root"));
    ATF_REQUIRE(lookup(".", "root", DT_REG));
    fs::cleanup(fs::path("root"));
    ATF_REQUIRE(!lookup(".", "root", DT_REG));
}


ATF_TEST_CASE_WITHOUT_HEAD(cleanup__subdir__empty);
ATF_TEST_CASE_BODY(cleanup__subdir__empty)
{
    fs::mkdir(fs::path("root"), 0755);
    ATF_REQUIRE(lookup(".", "root", DT_DIR));
    fs::cleanup(fs::path("root"));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(cleanup__subdir__files_and_directories);
ATF_TEST_CASE_BODY(cleanup__subdir__files_and_directories)
{
    fs::mkdir(fs::path("root"), 0755);
    utils::create_file(fs::path("root/.hidden_file"));
    fs::mkdir(fs::path("root/.hidden_dir"), 0755);
    utils::create_file(fs::path("root/.hidden_dir/a"));
    utils::create_file(fs::path("root/file"));
    utils::create_file(fs::path("root/with spaces"));
    fs::mkdir(fs::path("root/dir1"), 0755);
    fs::mkdir(fs::path("root/dir1/dir2"), 0755);
    utils::create_file(fs::path("root/dir1/dir2/file"));
    fs::mkdir(fs::path("root/dir1/dir3"), 0755);
    ATF_REQUIRE(lookup(".", "root", DT_DIR));
    fs::cleanup(fs::path("root"));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(cleanup__subdir__unprotect);
ATF_TEST_CASE_BODY(cleanup__subdir__unprotect)
{
    fs::mkdir(fs::path("root"), 0755);
    fs::mkdir(fs::path("root/foo"), 0755);
    utils::create_file(fs::path("root/foo/bar"));
    ATF_REQUIRE(::chmod("root/foo/bar", 0555) != -1);
    ATF_REQUIRE(::chmod("root/foo", 0555) != -1);
    fs::cleanup(fs::path("root"));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(cleanup__subdir__links);
ATF_TEST_CASE_BODY(cleanup__subdir__links)
{
    fs::mkdir(fs::path("test"), 0755);
    const bool lchmod_fails = (::lchmod("test", 0700) == -1 &&
                               ::chmod("test", 0700) != -1);

    fs::mkdir(fs::path("root"), 0755);
    fs::mkdir(fs::path("root/dir1"), 0755);
    ATF_REQUIRE(::symlink("../../root", "root/dir1/loop") != -1);
    ATF_REQUIRE(::symlink("non-existent", "root/missing") != -1);
    ATF_REQUIRE(lookup(".", "root", DT_DIR));
    try {
        fs::cleanup(fs::path("root"));
    } catch (const fs::error& e) {
        if (lchmod_fails)
            expect_fail("lchmod(2) is not implemented in your system");
        fail(e.what());
    }
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TEST_CASE(cleanup__mount_point__root__one);
ATF_TEST_CASE_HEAD(cleanup__mount_point__root__one)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(cleanup__mount_point__root__one)
{
    fs::mkdir(fs::path("root"), 0755);
    utils::mount_tmpfs(fs::path("root"));
    fs::cleanup(fs::path("root"));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TEST_CASE(cleanup__mount_point__root__many);
ATF_TEST_CASE_HEAD(cleanup__mount_point__root__many)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(cleanup__mount_point__root__many)
{
    fs::mkdir(fs::path("root"), 0755);
    utils::mount_tmpfs(fs::path("root"));
    utils::mount_tmpfs(fs::path("root"));
    fs::cleanup(fs::path("root"));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TEST_CASE(cleanup__mount_point__subdir__one);
ATF_TEST_CASE_HEAD(cleanup__mount_point__subdir__one)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(cleanup__mount_point__subdir__one)
{
    fs::mkdir(fs::path("root"), 0755);
    fs::mkdir(fs::path("root/dir1"), 0755);
    utils::create_file(fs::path("root/zz"));
    utils::mount_tmpfs(fs::path("root/dir1"));
    fs::cleanup(fs::path("root"));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TEST_CASE(cleanup__mount_point__subdir__many);
ATF_TEST_CASE_HEAD(cleanup__mount_point__subdir__many)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(cleanup__mount_point__subdir__many)
{
    fs::mkdir(fs::path("root"), 0755);
    fs::mkdir(fs::path("root/dir1"), 0755);
    utils::create_file(fs::path("root/zz"));
    utils::mount_tmpfs(fs::path("root/dir1"));
    utils::mount_tmpfs(fs::path("root/dir1"));
    fs::cleanup(fs::path("root"));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TEST_CASE(cleanup__mount_point__nested);
ATF_TEST_CASE_HEAD(cleanup__mount_point__nested)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(cleanup__mount_point__nested)
{
    fs::mkdir(fs::path("root"), 0755);
    fs::mkdir(fs::path("root/dir1"), 0755);
    fs::mkdir(fs::path("root/dir1/dir2"), 0755);
    fs::mkdir(fs::path("root/dir3"), 0755);
    utils::mount_tmpfs(fs::path("root/dir1/dir2"));
    utils::mount_tmpfs(fs::path("root/dir3"));
    fs::mkdir(fs::path("root/dir1/dir2/dir4"), 0755);
    utils::mount_tmpfs(fs::path("root/dir1/dir2/dir4"));
    fs::mkdir(fs::path("root/dir1/dir2/not-mount-point"), 0755);
    fs::cleanup(fs::path("root"));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TEST_CASE(cleanup__mount_point__links);
ATF_TEST_CASE_HEAD(cleanup__mount_point__links)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(cleanup__mount_point__links)
{
    fs::mkdir(fs::path("root"), 0755);
    fs::mkdir(fs::path("root/dir1"), 0755);
    fs::mkdir(fs::path("root/dir3"), 0755);
    utils::mount_tmpfs(fs::path("root/dir1"));
    ATF_REQUIRE(::symlink("../dir3", "root/dir1/link") != -1);
    fs::cleanup(fs::path("root"));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TEST_CASE(cleanup__mount_point__busy);
ATF_TEST_CASE_HEAD(cleanup__mount_point__busy)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(cleanup__mount_point__busy)
{
    fs::mkdir(fs::path("root"), 0755);
    fs::mkdir(fs::path("root/dir1"), 0755);
    utils::mount_tmpfs(fs::path("root/dir1"));

    pid_t pid = ::fork();
    ATF_REQUIRE(pid != -1);
    if (pid == 0) {
        if (::chdir("root/dir1") == -1)
            std::abort();

        utils::create_file(fs::path("dont-delete-me"));
        utils::create_file(fs::path("../../done"));

        ::pause();
        std::exit(EXIT_SUCCESS);
    } else {
        std::cerr << "Waiting for child to finish preparations\n";
        while (!fs::exists(fs::path("done"))) {}
        std::cerr << "Child done; cleaning up\n";

        ATF_REQUIRE_THROW(fs::error, fs::cleanup(fs::path("root")));
        ATF_REQUIRE(fs::exists(fs::path("root/dir1/dont-delete-me")));

        std::cerr << "Killing child\n";
        ATF_REQUIRE(::kill(pid, SIGKILL) != -1);
        int status;
        ATF_REQUIRE(::waitpid(pid, &status, 0) != -1);

        fs::cleanup(fs::path("root"));
        ATF_REQUIRE(!lookup(".", "root", DT_DIR));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(current_path__ok);
ATF_TEST_CASE_BODY(current_path__ok)
{
    const fs::path previous = fs::current_path();
    fs::mkdir(fs::path("root"), 0755);
    ATF_REQUIRE(::chdir("root") != -1);
    const fs::path cwd = fs::current_path();
    ATF_REQUIRE_EQ(cwd.str().length() - 5, cwd.str().find("/root"));
    ATF_REQUIRE_EQ(previous / "root", cwd);
}


ATF_TEST_CASE_WITHOUT_HEAD(current_path__enoent);
ATF_TEST_CASE_BODY(current_path__enoent)
{
    const fs::path previous = fs::current_path();
    fs::mkdir(fs::path("root"), 0755);
    ATF_REQUIRE(::chdir("root") != -1);
    ATF_REQUIRE(::rmdir("../root") != -1);
    try {
        (void)fs::current_path();
        fail("system_errpr not raised");
    } catch (const fs::system_error& e) {
        ATF_REQUIRE_EQ(ENOENT, e.original_errno());
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(exists);
ATF_TEST_CASE_BODY(exists)
{
    const fs::path dir("dir");
    ATF_REQUIRE(!fs::exists(dir));
    fs::mkdir(dir, 0755);
    ATF_REQUIRE(fs::exists(dir));
}


ATF_TEST_CASE_WITHOUT_HEAD(find_in_path__no_path);
ATF_TEST_CASE_BODY(find_in_path__no_path)
{
    utils::unsetenv("PATH");
    ATF_REQUIRE(!fs::find_in_path("ls"));
    utils::create_file(fs::path("ls"));
    ATF_REQUIRE(!fs::find_in_path("ls"));
}


ATF_TEST_CASE_WITHOUT_HEAD(find_in_path__empty_path);
ATF_TEST_CASE_BODY(find_in_path__empty_path)
{
    utils::setenv("PATH", "");
    ATF_REQUIRE(!fs::find_in_path("ls"));
    utils::create_file(fs::path("ls"));
    ATF_REQUIRE(!fs::find_in_path("ls"));
}


ATF_TEST_CASE_WITHOUT_HEAD(find_in_path__one_component);
ATF_TEST_CASE_BODY(find_in_path__one_component)
{
    const fs::path dir = fs::current_path() / "bin";
    fs::mkdir(dir, 0755);
    utils::setenv("PATH", dir.str());

    ATF_REQUIRE(!fs::find_in_path("ls"));
    utils::create_file(dir / "ls");
    ATF_REQUIRE_EQ(dir / "ls", fs::find_in_path("ls").get());
}


ATF_TEST_CASE_WITHOUT_HEAD(find_in_path__many_components);
ATF_TEST_CASE_BODY(find_in_path__many_components)
{
    const fs::path dir1 = fs::current_path() / "dir1";
    const fs::path dir2 = fs::current_path() / "dir2";
    fs::mkdir(dir1, 0755);
    fs::mkdir(dir2, 0755);
    utils::setenv("PATH", dir1.str() + ":" + dir2.str());

    ATF_REQUIRE(!fs::find_in_path("ls"));
    utils::create_file(dir2 / "ls");
    ATF_REQUIRE_EQ(dir2 / "ls", fs::find_in_path("ls").get());
    utils::create_file(dir1 / "ls");
    ATF_REQUIRE_EQ(dir1 / "ls", fs::find_in_path("ls").get());
}


ATF_TEST_CASE_WITHOUT_HEAD(find_in_path__current_directory);
ATF_TEST_CASE_BODY(find_in_path__current_directory)
{
    utils::setenv("PATH", "bin:");

    ATF_REQUIRE(!fs::find_in_path("foo-bar"));
    utils::create_file(fs::path("foo-bar"));
    ATF_REQUIRE_EQ(fs::path("foo-bar"), fs::find_in_path("foo-bar").get());
}


ATF_TEST_CASE_WITHOUT_HEAD(mkdir__ok);
ATF_TEST_CASE_BODY(mkdir__ok)
{
    fs::mkdir(fs::path("dir"), 0755);
    ATF_REQUIRE(lookup(".", "dir", DT_DIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(mkdir__enoent);
ATF_TEST_CASE_BODY(mkdir__enoent)
{
    try {
        fs::mkdir(fs::path("dir1/dir2"), 0755);
        fail("system_error not raised");
    } catch (const fs::system_error& e) {
        ATF_REQUIRE_EQ(ENOENT, e.original_errno());
    }
    ATF_REQUIRE(!lookup(".", "dir1", DT_DIR));
    ATF_REQUIRE(!lookup(".", "dir2", DT_DIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(mkdir_p__one_component);
ATF_TEST_CASE_BODY(mkdir_p__one_component)
{
    ATF_REQUIRE(!lookup(".", "new-dir", DT_DIR));
    fs::mkdir_p(fs::path("new-dir"), 0755);
    ATF_REQUIRE(lookup(".", "new-dir", DT_DIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(mkdir_p__many_components);
ATF_TEST_CASE_BODY(mkdir_p__many_components)
{
    ATF_REQUIRE(!lookup(".", "a", DT_DIR));
    fs::mkdir_p(fs::path("a/b/c"), 0755);
    ATF_REQUIRE(lookup(".", "a", DT_DIR));
    ATF_REQUIRE(lookup("a", "b", DT_DIR));
    ATF_REQUIRE(lookup("a/b", "c", DT_DIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(mkdir_p__already_exists);
ATF_TEST_CASE_BODY(mkdir_p__already_exists)
{
    fs::mkdir(fs::path("a"), 0755);
    fs::mkdir(fs::path("a/b"), 0755);
    fs::mkdir_p(fs::path("a/b"), 0755);
}


ATF_TEST_CASE(mkdir_p__eacces)
ATF_TEST_CASE_HEAD(mkdir_p__eacces)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(mkdir_p__eacces)
{
    fs::mkdir(fs::path("a"), 0755);
    fs::mkdir(fs::path("a/b"), 0755);
    ATF_REQUIRE(::chmod("a/b", 0555) != -1);
    try {
        fs::mkdir_p(fs::path("a/b/c/d"), 0755);
        fail("system_error not raised");
    } catch (const fs::system_error& e) {
        ATF_REQUIRE_EQ(EACCES, e.original_errno());
    }
    ATF_REQUIRE(lookup(".", "a", DT_DIR));
    ATF_REQUIRE(lookup("a", "b", DT_DIR));
    ATF_REQUIRE(!lookup(".", "c", DT_DIR));
    ATF_REQUIRE(!lookup("a", "c", DT_DIR));
    ATF_REQUIRE(!lookup("a/b", "c", DT_DIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(mkdtemp)
ATF_TEST_CASE_BODY(mkdtemp)
{
    const fs::path dir_template("tempdir.XXXXXX");
    const fs::path tempdir = fs::mkdtemp(dir_template);
    ATF_REQUIRE(!lookup(".", dir_template.c_str(), DT_DIR));
    ATF_REQUIRE(lookup(".", tempdir.c_str(), DT_DIR));
}


ATF_TEST_CASE(unmount__ok)
ATF_TEST_CASE_HEAD(unmount__ok)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(unmount__ok)
{
    const fs::path mount_point("mount_point");
    fs::mkdir(mount_point, 0755);

    utils::create_file(mount_point / "test1");
    utils::mount_tmpfs(mount_point);
    utils::create_file(mount_point / "test2");

    ATF_REQUIRE(!fs::exists(mount_point / "test1"));
    ATF_REQUIRE( fs::exists(mount_point / "test2"));
    fs::unmount(mount_point);
    ATF_REQUIRE( fs::exists(mount_point / "test1"));
    ATF_REQUIRE(!fs::exists(mount_point / "test2"));
}


ATF_TEST_CASE(unmount__fail)
ATF_TEST_CASE_HEAD(unmount__fail)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(unmount__fail)
{
    const fs::path mount_point("mount_point");

    ATF_REQUIRE_THROW(fs::error, fs::unmount(mount_point));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, cleanup__file);
    ATF_ADD_TEST_CASE(tcs, cleanup__subdir__empty);
    ATF_ADD_TEST_CASE(tcs, cleanup__subdir__files_and_directories);
    ATF_ADD_TEST_CASE(tcs, cleanup__subdir__unprotect);
    ATF_ADD_TEST_CASE(tcs, cleanup__subdir__links);
    ATF_ADD_TEST_CASE(tcs, cleanup__mount_point__root__one);
    ATF_ADD_TEST_CASE(tcs, cleanup__mount_point__root__many);
    ATF_ADD_TEST_CASE(tcs, cleanup__mount_point__subdir__one);
    ATF_ADD_TEST_CASE(tcs, cleanup__mount_point__subdir__many);
    ATF_ADD_TEST_CASE(tcs, cleanup__mount_point__nested);
    ATF_ADD_TEST_CASE(tcs, cleanup__mount_point__links);
    ATF_ADD_TEST_CASE(tcs, cleanup__mount_point__busy);

    ATF_ADD_TEST_CASE(tcs, current_path__ok);
    ATF_ADD_TEST_CASE(tcs, current_path__enoent);

    ATF_ADD_TEST_CASE(tcs, exists);

    ATF_ADD_TEST_CASE(tcs, find_in_path__no_path);
    ATF_ADD_TEST_CASE(tcs, find_in_path__empty_path);
    ATF_ADD_TEST_CASE(tcs, find_in_path__one_component);
    ATF_ADD_TEST_CASE(tcs, find_in_path__many_components);
    ATF_ADD_TEST_CASE(tcs, find_in_path__current_directory);

    ATF_ADD_TEST_CASE(tcs, mkdir__ok);
    ATF_ADD_TEST_CASE(tcs, mkdir__enoent);

    ATF_ADD_TEST_CASE(tcs, mkdir_p__one_component);
    ATF_ADD_TEST_CASE(tcs, mkdir_p__many_components);
    ATF_ADD_TEST_CASE(tcs, mkdir_p__already_exists);
    ATF_ADD_TEST_CASE(tcs, mkdir_p__eacces);

    ATF_ADD_TEST_CASE(tcs, mkdtemp);

    ATF_ADD_TEST_CASE(tcs, unmount__ok);
    ATF_ADD_TEST_CASE(tcs, unmount__fail);
}
