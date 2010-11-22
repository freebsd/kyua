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
#include <sys/stat.h>

#include <dirent.h>
}

#include <cerrno>
#include <cstring>
#include <fstream>

#include <atf-c++.hpp>

#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"

namespace fs = utils::fs;


namespace {


/// Creates a file for testing.
///
/// Fails the test case if the file cannot be created.
///
/// \param file The name of the file to create.
static void
create_file(const char* file)
{
    std::ofstream output(file);
    if (!output)
        ATF_FAIL(F("Failed to create test file %s") % file);
    output << "Some contents\n";
}


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
        if (dp->d_namlen == std::strlen(name) &&
            std::strcmp(dp->d_name, name) == 0 &&
            dp->d_type == expected_type) {
            found = true;
        }
    }
    ::closedir(dirp);
    return found;
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(cleanup__empty);
ATF_TEST_CASE_BODY(cleanup__empty)
{
    fs::mkdir(fs::path("root"), 0755);
    ATF_REQUIRE(lookup(".", "root", DT_DIR));
    fs::cleanup(fs::path("root"));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(cleanup__files_and_directories);
ATF_TEST_CASE_BODY(cleanup__files_and_directories)
{
    fs::mkdir(fs::path("root"), 0755);
    create_file("root/.hidden_file");
    fs::mkdir(fs::path("root/.hidden_dir"), 0755);
    create_file("root/.hidden_dir/a");
    create_file("root/file");
    create_file("root/with spaces");
    fs::mkdir(fs::path("root/dir1"), 0755);
    fs::mkdir(fs::path("root/dir1/dir2"), 0755);
    create_file("root/dir1/dir2/file");
    fs::mkdir(fs::path("root/dir1/dir3"), 0755);
    ATF_REQUIRE(lookup(".", "root", DT_DIR));
    fs::cleanup(fs::path("root"));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
}


ATF_TEST_CASE_WITHOUT_HEAD(cleanup__unprotect);
ATF_TEST_CASE_BODY(cleanup__unprotect)
{
    fs::mkdir(fs::path("root"), 0755);
    fs::mkdir(fs::path("root/foo"), 0755);
    create_file("root/foo/bar");
    ATF_REQUIRE(::chmod("root/foo/bar", 0555) != -1);
    ATF_REQUIRE(::chmod("root/foo", 0555) != -1);
    fs::cleanup(fs::path("root"));
    ATF_REQUIRE(!lookup(".", "root", DT_DIR));
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


ATF_TEST_CASE_WITHOUT_HEAD(mkdir_p__eacces)
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


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, cleanup__empty);
    ATF_ADD_TEST_CASE(tcs, cleanup__files_and_directories);
    ATF_ADD_TEST_CASE(tcs, cleanup__unprotect);

    ATF_ADD_TEST_CASE(tcs, current_path__ok);
    ATF_ADD_TEST_CASE(tcs, current_path__enoent);

    ATF_ADD_TEST_CASE(tcs, mkdir__ok);
    ATF_ADD_TEST_CASE(tcs, mkdir__enoent);

    ATF_ADD_TEST_CASE(tcs, mkdir_p__one_component);
    ATF_ADD_TEST_CASE(tcs, mkdir_p__many_components);
    ATF_ADD_TEST_CASE(tcs, mkdir_p__eacces);

    ATF_ADD_TEST_CASE(tcs, mkdtemp);
}
