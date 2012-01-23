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

#include "engine/plain_iface/test_case.hpp"

extern "C" {
#include <sys/stat.h>

#include <signal.h>
#include <unistd.h>
}

#include <cerrno>
#include <fstream>
#include <iostream>

#include <atf-c++.hpp>

#include "engine/exceptions.hpp"
#include "engine/plain_iface/test_program.hpp"
#include "engine/test_result.hpp"
#include "engine/user_files/config.hpp"
#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/format/macros.hpp"
#include "utils/optional.ipp"
#include "utils/process/children.ipp"
#include "utils/sanity.hpp"
#include "utils/test_utils.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace plain_iface = engine::plain_iface;
namespace process = utils::process;
namespace user_files = engine::user_files;

using utils::none;
using utils::optional;


namespace {


/// Simplifies the execution of the helper test cases.
class plain_helper {
    /// Path to the test program's source directory.
    const fs::path _srcdir;

    /// The root of the test suite.
    fs::path _root;

    /// Path to the helper test program, relative to _root.
    fs::path _binary_path;

    /// Optional timeout for the test program.
    optional< datetime::delta > _timeout;

public:
    /// Constructs a new helper.
    ///
    /// \param atf_tc A pointer to the calling test case.  Needed to obtain
    ///     run-time configuration variables.
    /// \param name The name of the helper to run.
    /// \param timeout An optional timeout for the test case.
    plain_helper(const atf::tests::tc* atf_tc, const char* name,
                 const optional< datetime::delta > timeout = none) :
        _srcdir(atf_tc->get_config_var("srcdir")),
        _root(_srcdir),
        _binary_path("test_case_helpers"),
        _timeout(timeout)
    {
        utils::setenv("TEST_CASE", name);
    }

    /// Sets an environment variable for the helper.
    ///
    /// This is simply syntactic sugar for utils::setenv.
    ///
    /// \param variable The name of the environment variable to set.
    /// \param value The value of the variable; must be convertible to a string.
    template< typename T >
    void
    set(const char* variable, const T& value)
    {
        utils::setenv(variable, F("%s") % value);
    }

    /// Places the helper in a different location.
    ///
    /// This prepares the helper to be run from a different location than the
    /// source directory so that the runtime execution can be validated.
    ///
    /// \param new_binary_path The new path to the binary, relative to the test
    ///     suite root.
    /// \param new_root The new test suite root.
    ///
    /// \pre The directory holding the target test program must exist.
    ///     Otherwise, the relocation of the binary will fail.
    void
    move(const char* new_binary_path, const char* new_root)
    {
        _binary_path = fs::path(new_binary_path);
        _root = fs::path(new_root);

        const fs::path src_path = fs::path(_srcdir) / "test_case_helpers";
        const fs::path new_path = _root / _binary_path;
        ATF_REQUIRE(
            ::symlink(src_path.c_str(), new_path.c_str()) != -1);
    }

    /// Runs the helper.
    ///
    /// \param config The runtime engine configuration, if different to the
    /// defaults.
    ///
    /// \return The result of the execution.
    engine::test_result
    run(const user_files::config& config = user_files::config::defaults()) const
    {
        const plain_iface::test_program test_program(_binary_path, _root,
                                                     "unit-tests", _timeout);
        const plain_iface::test_case test_case(test_program);
        engine::test_case_hooks dummy_hooks;
        return test_case.run(config, dummy_hooks);
    }
};


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(ctor);
ATF_TEST_CASE_BODY(ctor)
{
    const plain_iface::test_program test_program(fs::path("program"),
                                                 fs::path("root"),
                                                 "test-suite", none);
    const plain_iface::test_case test_case(test_program);
    ATF_REQUIRE(&test_program == &test_case.test_program());
    ATF_REQUIRE_EQ("main", test_case.name());
}


ATF_TEST_CASE_WITHOUT_HEAD(all_properties__none);
ATF_TEST_CASE_BODY(all_properties__none)
{
    const plain_iface::test_program test_program(fs::path("program"),
                                                 fs::path("root"),
                                                 "test-suite", none);
    const plain_iface::test_case test_case(test_program);
    ATF_REQUIRE(test_case.all_properties().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(all_properties__all);
ATF_TEST_CASE_BODY(all_properties__all)
{
    const plain_iface::test_program test_program(
        fs::path("program"), fs::path("root"), "test-suite",
        utils::make_optional(datetime::delta(123, 0)));
    const plain_iface::test_case test_case(test_program);

    engine::properties_map exp_properties;
    exp_properties["timeout"] = "123";

    ATF_REQUIRE(exp_properties == test_case.all_properties());
}


ATF_TEST_CASE_WITHOUT_HEAD(run__result_pass);
ATF_TEST_CASE_BODY(run__result_pass)
{
    ATF_REQUIRE(engine::test_result(engine::test_result::passed) ==
                plain_helper(this, "pass").run());
}


ATF_TEST_CASE_WITHOUT_HEAD(run__result_fail);
ATF_TEST_CASE_BODY(run__result_fail)
{
    ATF_REQUIRE(engine::test_result(engine::test_result::failed,
                                    "Exited with code 8") ==
                plain_helper(this, "fail").run());
}


ATF_TEST_CASE_WITHOUT_HEAD(run__result_crash);
ATF_TEST_CASE_BODY(run__result_crash)
{
    const engine::test_result result = plain_helper(this, "crash").run();
    ATF_REQUIRE(engine::test_result::broken == result.type());
    ATF_REQUIRE_MATCH(F("Received signal %s") % SIGABRT, result.reason());
}


ATF_TEST_CASE_WITHOUT_HEAD(run__current_directory);
ATF_TEST_CASE_BODY(run__current_directory)
{
    plain_helper helper(this, "pass");
    helper.move("program", ".");
    ATF_REQUIRE(engine::test_result(engine::test_result::passed) ==
                helper.run());
}


ATF_TEST_CASE_WITHOUT_HEAD(run__subdirectory);
ATF_TEST_CASE_BODY(run__subdirectory)
{
    plain_helper helper(this, "pass");
    ATF_REQUIRE(::mkdir("dir1", 0755) != -1);
    ATF_REQUIRE(::mkdir("dir1/dir2", 0755) != -1);
    helper.move("dir2/program", "dir1");
    ATF_REQUIRE(engine::test_result(engine::test_result::passed) ==
                helper.run());
}


ATF_TEST_CASE_WITHOUT_HEAD(run__kill_children);
ATF_TEST_CASE_BODY(run__kill_children)
{
    plain_helper helper(this, "spawn_blocking_child");
    helper.set("CONTROL_DIR", fs::current_path());
    ATF_REQUIRE(engine::test_result(engine::test_result::passed) ==
                helper.run());

    if (!fs::exists(fs::path("pid")))
        fail("The pid file was not created");
    std::ifstream pidfile("pid");
    ATF_REQUIRE(pidfile);
    pid_t pid;
    pidfile >> pid;
    pidfile.close();

    int attempts = 3;
retry:
    if (::kill(pid, SIGCONT) != -1 || errno != ESRCH) {
        // Looks like the subchild did not die.
        //
        // Note that this might be inaccurate for two reasons:
        // 1) The system may have spawned a new process with the same pid as
        //    our subchild... but in practice, this does not happen because
        //    most systems do not immediately reuse pid numbers.  If that
        //    happens... well, we get a false test failure.
        // 2) We ran so fast that even if the process was sent a signal to
        //    die, it has not had enough time to process it yet.  This is why
        //    we retry this a few times.
        if (attempts > 0) {
            std::cout << "Subprocess not dead yet; retrying wait\n";
            --attempts;
            ::sleep(1);
            goto retry;
        }
        fail(F("The subprocess %s of our child was not killed") % pid);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(run__isolation);
ATF_TEST_CASE_BODY(run__isolation)
{
    const plain_helper helper(this, "validate_isolation");
    utils::setenv("TEST_CASE", "validate_isolation");
    // Simple checks to make sure that isolate_process has been called.
    utils::setenv("HOME", "foobar");
    utils::setenv("LANG", "C");
    ATF_REQUIRE(engine::test_result(engine::test_result::passed) ==
                helper.run());
}


ATF_TEST_CASE_WITHOUT_HEAD(run__timeout);
ATF_TEST_CASE_BODY(run__timeout)
{
    plain_helper helper(this, "timeout",
                        utils::make_optional(datetime::delta(1, 0)));
    helper.set("CONTROL_DIR", fs::current_path());
    ATF_REQUIRE(engine::test_result(engine::test_result::broken,
                                    "Test case timed out") ==
                helper.run());

    if (fs::exists(fs::path("cookie")))
        fail("It seems that the test case was not killed after it timed out");
}


ATF_TEST_CASE_WITHOUT_HEAD(run__missing_test_program);
ATF_TEST_CASE_BODY(run__missing_test_program)
{
    plain_helper helper(this, "pass");
    ATF_REQUIRE(::mkdir("dir", 0755) != -1);
    helper.move("test_case_helpers", "dir");
    ATF_REQUIRE(::unlink("dir/test_case_helpers") != -1);
    const engine::test_result result = helper.run();
    ATF_REQUIRE(engine::test_result::broken == result.type());
    ATF_REQUIRE_MATCH("Failed to execute", result.reason());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, ctor);
    ATF_ADD_TEST_CASE(tcs, all_properties__none);
    ATF_ADD_TEST_CASE(tcs, all_properties__all);

    ATF_ADD_TEST_CASE(tcs, run__result_pass);
    ATF_ADD_TEST_CASE(tcs, run__result_fail);
    ATF_ADD_TEST_CASE(tcs, run__result_crash);
    ATF_ADD_TEST_CASE(tcs, run__current_directory);
    ATF_ADD_TEST_CASE(tcs, run__subdirectory);
    ATF_ADD_TEST_CASE(tcs, run__kill_children);
    ATF_ADD_TEST_CASE(tcs, run__isolation);
    ATF_ADD_TEST_CASE(tcs, run__timeout);
    ATF_ADD_TEST_CASE(tcs, run__missing_test_program);
}
