// Copyright 2012 Google Inc.
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

#include "utils/stacktrace.hpp"

extern "C" {
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <signal.h>
#include <unistd.h>
}

#include <iostream>
#include <sstream>

#include <atf-c++.hpp>

#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/process/children.ipp"
#include "utils/process/status.hpp"
#include "utils/sanity.hpp"
#include "utils/test_utils.hpp"

namespace fs = utils::fs;
namespace process = utils::process;

using utils::none;
using utils::optional;


namespace {


/// Functor to execute a binary in a subprocess.
class crash_me {
    /// Path to the binary to execute.
    const fs::path _binary;

public:
    /// Constructor.
    ///
    /// \param binary_ Path to binary to execute.
    explicit crash_me(const char* binary_) : _binary(binary_)
    {
    }

    /// Runs the binary.
    void
    operator()(void)
    {
        const std::vector< std::string > args;
        process::exec(_binary, args);
    }
};


/// Generates a core dump, if possible.
///
/// \post If this fails to generate a core file, the test case is marked as
/// skipped.  The caller can rely on this when attempting further checks on the
/// core dump by assuming that the core dump exists somewhere.
///
/// \param test_case Pointer to the caller test case, needed to obtain the path
///     to the source directory.
/// \param base_name Name of the binary to execute, which will be a copy of a
///     helper binary that always crashes.  This name should later be part of
///     the core filename.
///
/// \return The status of the crashed binary.
static process::status
generate_core(const atf::tests::tc* test_case, const char* base_name)
{
    utils::unlimit_core_size();

    const fs::path helper = fs::path(test_case->get_config_var("srcdir")) /
        "stacktrace_helper";
    utils::copy_file(helper, fs::path(base_name));

    const process::status status = process::child_with_files::fork(
        crash_me(base_name),
        fs::path("unused.out"), fs::path("unused.err"))->wait();
    ATF_REQUIRE(status.signaled());
    if (!status.coredump())
        ATF_SKIP("Test failed to generate core dump");
    return status;
}


/// Creates a script.
///
/// \param script Path to the script to create.
/// \param contents Contents of the script.
static void
create_script(const char* script, const std::string& contents)
{
    utils::create_file(fs::path(script), "#! /bin/sh\n\n" + contents);
    ATF_REQUIRE(::chmod(script, 0755) != -1);
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(unlimit_core_size);
ATF_TEST_CASE_BODY(unlimit_core_size)
{
    struct rlimit rl;
    rl.rlim_cur = 0;
    rl.rlim_max = RLIM_INFINITY;
    if (::setrlimit(RLIMIT_CORE, &rl) == -1)
        skip("Failed to lower the core size limit");

    utils::unlimit_core_size();

    const fs::path helper = fs::path(get_config_var("srcdir")) /
        "stacktrace_helper";
    const process::status status = process::child_with_files::fork(
        crash_me(helper.c_str()),
        fs::path("unused.out"), fs::path("unused.err"))->wait();
    ATF_REQUIRE(status.signaled());
    if (!status.coredump())
        fail("Core not dumped as expected");
}


ATF_TEST_CASE_WITHOUT_HEAD(find_gdb__use_builtin);
ATF_TEST_CASE_BODY(find_gdb__use_builtin)
{
    utils::builtin_gdb = "/path/to/gdb";
    optional< fs::path > gdb = utils::find_gdb();
    ATF_REQUIRE(gdb);
    ATF_REQUIRE_EQ("/path/to/gdb", gdb.get().str());
}


ATF_TEST_CASE_WITHOUT_HEAD(find_gdb__search_builtin__ok);
ATF_TEST_CASE_BODY(find_gdb__search_builtin__ok)
{
    utils::create_file(fs::path("custom-name"));
    ATF_REQUIRE(::chmod("custom-name", 0755) != -1);
    const fs::path exp_gdb = fs::path("custom-name").to_absolute();

    utils::setenv("PATH", "/non-existent/location:.:/bin");

    utils::builtin_gdb = "custom-name";
    optional< fs::path > gdb = utils::find_gdb();
    ATF_REQUIRE(gdb);
    ATF_REQUIRE_EQ(exp_gdb, gdb.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(find_gdb__search_builtin__fail);
ATF_TEST_CASE_BODY(find_gdb__search_builtin__fail)
{
    utils::setenv("PATH", ".");
    utils::builtin_gdb = "foo";
    optional< fs::path > gdb = utils::find_gdb();
    ATF_REQUIRE(!gdb);
}


ATF_TEST_CASE_WITHOUT_HEAD(find_gdb__bogus_value);
ATF_TEST_CASE_BODY(find_gdb__bogus_value)
{
    utils::builtin_gdb = "";
    optional< fs::path > gdb = utils::find_gdb();
    ATF_REQUIRE(!gdb);
}


ATF_TEST_CASE_WITHOUT_HEAD(find_core__found__short);
ATF_TEST_CASE_BODY(find_core__found__short)
{
    const process::status status = generate_core(this, "short");
    INV(status.coredump());
    const optional< fs::path > core_name = utils::find_core(
        fs::path("short"), status, fs::path("."));
    if (!core_name)
        fail("Core dumped, but no candidates found");
    ATF_REQUIRE(core_name.get().str().find("core") != std::string::npos);
    ATF_REQUIRE(fs::exists(core_name.get()));
}


ATF_TEST_CASE_WITHOUT_HEAD(find_core__found__long);
ATF_TEST_CASE_BODY(find_core__found__long)
{
    const process::status status = generate_core(
        this, "long-name-that-may-be-truncated-in-some-systems");
    INV(status.coredump());
    const optional< fs::path > core_name = utils::find_core(
        fs::path("long-name-that-may-be-truncated-in-some-systems"),
        status, fs::path("."));
    if (!core_name)
        fail("Core dumped, but no candidates found");
    ATF_REQUIRE(core_name.get().str().find("core") != std::string::npos);
    ATF_REQUIRE(fs::exists(core_name.get()));
}


ATF_TEST_CASE_WITHOUT_HEAD(find_core__not_found);
ATF_TEST_CASE_BODY(find_core__not_found)
{
    const process::status status = process::status::fake_signaled(SIGILL, true);
    const optional< fs::path > core_name = utils::find_core(
        fs::path("missing"), status, fs::path("."));
    if (core_name)
        fail("Core not dumped, but candidate found: " + core_name.get().str());
}


ATF_TEST_CASE_WITHOUT_HEAD(dump_stacktrace__integration);
ATF_TEST_CASE_BODY(dump_stacktrace__integration)
{
    const process::status status = generate_core(this, "short");
    INV(status.coredump());

    std::ostringstream output;
    utils::dump_stacktrace(fs::path("short"), status, fs::path("."), output);
    std::cout << output.str();

    // It is hard to validate the execution of an arbitrary GDB of which we do
    // not know anything.  Just assume that the backtrace, at the very least,
    // prints a frame identifier.
    ATF_REQUIRE_MATCH("#0", output.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(dump_stacktrace__ok);
ATF_TEST_CASE_BODY(dump_stacktrace__ok)
{
    utils::setenv("PATH", ".");
    create_script("fake-gdb", "echo 'frame 1'; echo 'frame 2'; "
                  "echo 'some warning' 1>&2; exit 0");
    utils::builtin_gdb = "fake-gdb";

    const process::status status = generate_core(this, "short");
    INV(status.coredump());

    std::ostringstream output;
    utils::dump_stacktrace(fs::path("short"), status, fs::path("."), output);
    std::cout << output.str();

    ATF_REQUIRE_MATCH("exited with signal [0-9]* and dumped core",
                      output.str());
    ATF_REQUIRE_MATCH("gdb stdout: frame 1", output.str());
    ATF_REQUIRE_MATCH("gdb stdout: frame 2", output.str());
    ATF_REQUIRE_MATCH("gdb stderr: some warning", output.str());
    ATF_REQUIRE_MATCH("GDB exited successfully", output.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(dump_stacktrace__cannot_find_core);
ATF_TEST_CASE_BODY(dump_stacktrace__cannot_find_core)
{
    const process::status status = process::status::fake_signaled(SIGILL, true);

    std::ostringstream output;
    utils::dump_stacktrace(fs::path("fake"), status, fs::path("."), output);
    std::cout << output.str();

    ATF_REQUIRE_MATCH("Cannot find any core file", output.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(dump_stacktrace__cannot_find_gdb);
ATF_TEST_CASE_BODY(dump_stacktrace__cannot_find_gdb)
{
    utils::setenv("PATH", ".");
    utils::builtin_gdb = "missing-gdb";

    const process::status status = process::status::fake_signaled(SIGILL, true);

    std::ostringstream output;
    utils::dump_stacktrace(fs::path("fake"), status, fs::path("."), output);
    std::cout << output.str();

    ATF_REQUIRE_MATCH("Cannot find GDB binary; builtin was 'missing-gdb'",
                      output.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(dump_stacktrace__gdb_fail);
ATF_TEST_CASE_BODY(dump_stacktrace__gdb_fail)
{
    utils::setenv("PATH", ".");
    create_script("fake-gdb", "echo 'foo'; echo 'bar' 1>&2; exit 1");
    utils::builtin_gdb = "fake-gdb";

    const process::status status = process::status::fake_signaled(SIGILL, true);
    utils::create_file(fs::path("fake.core"), "");

    std::ostringstream output;
    utils::dump_stacktrace(fs::path("fake"), status, fs::path("."), output);
    std::cout << output.str();

    ATF_REQUIRE_MATCH("gdb stdout: foo", output.str());
    ATF_REQUIRE_MATCH("gdb stderr: bar", output.str());
    ATF_REQUIRE_MATCH("GDB failed; see output above for details", output.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(dump_stacktrace_if_available__append);
ATF_TEST_CASE_BODY(dump_stacktrace_if_available__append)
{
    utils::setenv("PATH", ".");
    create_script("fake-gdb", "echo 'frame 1'; exit 0");
    utils::builtin_gdb = "fake-gdb";

    utils::create_file(fs::path("output.txt"), "Pre-contents");
    const process::status status = generate_core(this, "short");
    utils::dump_stacktrace_if_available(
        fs::path("short"), utils::make_optional(status), fs::path("."),
        fs::path("output.txt"));

    ATF_REQUIRE(utils::grep_file("Pre-contents", fs::path("output.txt")));
    ATF_REQUIRE(utils::grep_file("frame 1", fs::path("output.txt")));
}


ATF_TEST_CASE_WITHOUT_HEAD(dump_stacktrace_if_available__create);
ATF_TEST_CASE_BODY(dump_stacktrace_if_available__create)
{
    utils::setenv("PATH", ".");
    create_script("fake-gdb", "echo 'frame 1'; exit 0");
    utils::builtin_gdb = "fake-gdb";

    const process::status status = generate_core(this, "short");
    utils::dump_stacktrace_if_available(
        fs::path("short"), utils::make_optional(status), fs::path("."),
        fs::path("output.txt"));

    ATF_REQUIRE(utils::grep_file("frame 1", fs::path("output.txt")));
}


ATF_TEST_CASE_WITHOUT_HEAD(dump_stacktrace_if_available__no_status);
ATF_TEST_CASE_BODY(dump_stacktrace_if_available__no_status)
{
    utils::dump_stacktrace_if_available(fs::path("short"), none,
                                        fs::path("."), fs::path("output.txt"));
    ATF_REQUIRE(!fs::exists(fs::path("output.txt")));
}


ATF_TEST_CASE_WITHOUT_HEAD(dump_stacktrace_if_available__no_coredump);
ATF_TEST_CASE_BODY(dump_stacktrace_if_available__no_coredump)
{
    const process::status not_signaled = process::status::fake_exited(123);
    utils::dump_stacktrace_if_available(
        fs::path("short"), utils::make_optional(not_signaled), fs::path("."),
        fs::path("output.txt"));
    ATF_REQUIRE(!fs::exists(fs::path("output.txt")));

    const process::status no_core = process::status::fake_signaled(1, false);
    utils::dump_stacktrace_if_available(
        fs::path("short"), utils::make_optional(no_core), fs::path("."),
        fs::path("output.txt"));
    ATF_REQUIRE(!fs::exists(fs::path("output.txt")));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, unlimit_core_size);

    ATF_ADD_TEST_CASE(tcs, find_gdb__use_builtin);
    ATF_ADD_TEST_CASE(tcs, find_gdb__search_builtin__ok);
    ATF_ADD_TEST_CASE(tcs, find_gdb__search_builtin__fail);
    ATF_ADD_TEST_CASE(tcs, find_gdb__bogus_value);

    ATF_ADD_TEST_CASE(tcs, find_core__found__short);
    ATF_ADD_TEST_CASE(tcs, find_core__found__long);
    ATF_ADD_TEST_CASE(tcs, find_core__not_found);

    ATF_ADD_TEST_CASE(tcs, dump_stacktrace__integration);
    ATF_ADD_TEST_CASE(tcs, dump_stacktrace__ok);
    ATF_ADD_TEST_CASE(tcs, dump_stacktrace__cannot_find_core);
    ATF_ADD_TEST_CASE(tcs, dump_stacktrace__cannot_find_gdb);
    ATF_ADD_TEST_CASE(tcs, dump_stacktrace__gdb_fail);

    ATF_ADD_TEST_CASE(tcs, dump_stacktrace_if_available__append);
    ATF_ADD_TEST_CASE(tcs, dump_stacktrace_if_available__create);
    ATF_ADD_TEST_CASE(tcs, dump_stacktrace_if_available__no_status);
    ATF_ADD_TEST_CASE(tcs, dump_stacktrace_if_available__no_coredump);
}
