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

#include "engine/atf_iface/runner.hpp"

extern "C" {
#include <sys/stat.h>

#include <signal.h>
#include <unistd.h>
}

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <atf-c++.hpp>

#include "engine/exceptions.hpp"
#include "engine/test_result.hpp"
#include "engine/user_files/config.hpp"
#include "engine/user_files/kyuafile.hpp"
#include "utils/config/tree.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/env.hpp"
#include "utils/noncopyable.hpp"
#include "utils/passwd.hpp"
#include "utils/process/children.ipp"

namespace config = utils::config;
namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace process = utils::process;
namespace user_files = engine::user_files;


namespace {


/// Reads a file in memory, line by line.
///
/// \param file The file to read.
///
/// \return All the lines in the file.
std::vector< std::string >
read_lines(const fs::path& file)
{
    std::ifstream input(file.c_str());
    ATF_REQUIRE(input);

    std::vector< std::string > lines;
    std::string line;
    while (std::getline(input, line).good())
        lines.push_back(line);
    return lines;
}


/// Test case hooks to capture stdout and stderr in memory.
class capture_hooks : public engine::test_case_hooks {
public:
    /// Contents of the stdout of the test case.
    std::vector< std::string > stdout_lines;

    /// Contents of the stderr of the test case.
    std::vector< std::string > stderr_lines;

    /// Stores the stdout of the test case into stdout_lines.
    ///
    /// \param file The path to the file containing the stdout.
    void
    got_stdout(const fs::path& file)
    {
        atf::utils::cat_file(file.str(), "helper stdout:");
        ATF_REQUIRE(stdout_lines.empty());
        stdout_lines = read_lines(file);
    }

    /// Stores the stderr of the test case into stderr_lines.
    ///
    /// \param file The path to the file containing the stderr.
    void
    got_stderr(const fs::path& file)
    {
        atf::utils::cat_file(file.str(), "helper stderr:");
        ATF_REQUIRE(stderr_lines.empty());
        stderr_lines = read_lines(file);
    }
};


/// Launcher for the helper test cases.
///
/// This builder class can be used to construct the runtime state of the helper
/// test cases and later run them.  The class also provides other helper methods
/// to interact with the helper binary.
class atf_helper : utils::noncopyable {
    /// Path to the test program's source directory.
    const fs::path _srcdir;

    /// The root of the test suite.
    fs::path _root;

    /// Path to the helper test program, relative to _root.
    fs::path _binary_path;

    /// Name of the helper test case to run.
    const std::string _name;

    /// Metadata of the test case.
    engine::metadata_builder _mdbuilder;

    /// Run-time configuration for the test case.
    config::tree _user_config;

public:
    /// Constructs a new helper.
    ///
    /// \param atf_tc A pointer to the calling test case.  Needed to obtain
    ///     run-time configuration variables.
    /// \param name The name of the helper to run.
    atf_helper(const atf::tests::tc* atf_tc, const char* name) :
        _srcdir(atf_tc->get_config_var("srcdir")),
        _root(_srcdir),
        _binary_path("runner_helpers"),
        _name(name),
        _user_config(user_files::default_config())
    {
        _user_config.set_string("architecture", "mock-architecture");
        _user_config.set_string("platform", "mock-platform");
    }

    /// Provides raw access to the run-time configuration.
    ///
    /// To override test-suite-specific variables, use set_config() as it
    /// abstracts away the name of the fake test suite.
    ///
    /// \returns A reference to the test case configuration.
    config::tree&
    config(void)
    {
        return _user_config;
    }

    /// Sets a test-suite-specific configuration variable for the helper.
    ///
    /// \param variable The name of the environment variable to set.
    /// \param value The value of the variable; must be convertible to a string.
    template< typename T >
    void
    set_config(const char* variable, const T& value)
    {
        _user_config.set_string(F("test_suites.the-suite.%s") % variable,
                                F("%s") % value);
    }

    /// Sets a metadata variable for the helper.
    ///
    /// \param variable The name of the environment variable to set.
    /// \param value The value of the variable; must be convertible to a string.
    template< typename T >
    void
    set_metadata(const char* variable, const T& value)
    {
        _mdbuilder.set_string(variable, F("%s") % value);
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

        const fs::path src_path = fs::path(_srcdir / "runner_helpers");
        const fs::path new_path = _root / _binary_path;
        ATF_REQUIRE(
            ::symlink(src_path.c_str(), new_path.c_str()) != -1);
    }

    /// Runs the helper.
    ///
    /// \return The result of the execution.
    engine::test_result
    run(void) const
    {
        engine::test_case_hooks dummy_hooks;
        return run(dummy_hooks);
    }

    /// Runs the helper.
    ///
    /// \param hooks The hooks to pass to the test case.
    ///
    /// \return The result of the execution.
    engine::test_result
    run(engine::test_case_hooks& hooks) const
    {
        const engine::test_program test_program(
            "atf", _binary_path, _root, "the-suite",
            engine::metadata_builder().build());
        const engine::test_case test_case("atf", test_program, _name,
                                          _mdbuilder.build());
        return engine::run_test_case(&test_case, _user_config, hooks);
    }
};


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__current_directory);
ATF_TEST_CASE_BODY(run_test_case__current_directory)
{
    atf_helper helper(this, "pass");
    helper.move("program", ".");
    ATF_REQUIRE(engine::test_result(engine::test_result::passed) ==
                helper.run());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__subdirectory);
ATF_TEST_CASE_BODY(run_test_case__subdirectory)
{
    atf_helper helper(this, "pass");
    ATF_REQUIRE(::mkdir("dir1", 0755) != -1);
    ATF_REQUIRE(::mkdir("dir1/dir2", 0755) != -1);
    helper.move("dir2/program", "dir1");
    ATF_REQUIRE(engine::test_result(engine::test_result::passed) ==
                helper.run());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__config_variables);
ATF_TEST_CASE_BODY(run_test_case__config_variables)
{
    atf_helper helper(this, "create_cookie_in_control_dir");
    helper.set_config("control_dir", fs::current_path());
    ATF_REQUIRE(engine::test_result(engine::test_result::passed) ==
                helper.run());

    if (!fs::exists(fs::path("cookie")))
        fail("The cookie was not created where we expected; the test program "
             "probably received an invalid configuration variable");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__cleanup_shares_workdir);
ATF_TEST_CASE_BODY(run_test_case__cleanup_shares_workdir)
{
    atf_helper helper(this, "check_cleanup_workdir");
    helper.set_metadata("has_cleanup", "true");
    helper.set_config("control_dir", fs::current_path());
    ATF_REQUIRE(engine::test_result(engine::test_result::skipped,
                                    "cookie created") == helper.run());

    if (fs::exists(fs::path("missing_cookie")))
        fail("The cleanup part did not see the cookie; the work directory "
             "is probably not shared");
    if (fs::exists(fs::path("invalid_cookie")))
        fail("The cleanup part read an invalid cookie");
    if (!fs::exists(fs::path("cookie_ok")))
        fail("The cleanup part was not executed");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__has_cleanup__false);
ATF_TEST_CASE_BODY(run_test_case__has_cleanup__false)
{
    atf_helper helper(this, "create_cookie_from_cleanup");
    helper.set_metadata("has_cleanup", "false");
    helper.set_config("control_dir", fs::current_path());
    ATF_REQUIRE(engine::test_result(engine::test_result::passed) ==
                helper.run());

    if (fs::exists(fs::path("cookie")))
        fail("The cleanup part was executed even though the test case set "
             "has.cleanup to false");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__has_cleanup__true);
ATF_TEST_CASE_BODY(run_test_case__has_cleanup__true)
{
    atf_helper helper(this, "create_cookie_from_cleanup");
    helper.set_metadata("has_cleanup", "true");
    helper.set_config("control_dir", fs::current_path());
    ATF_REQUIRE(engine::test_result(engine::test_result::passed) ==
                helper.run());

    if (!fs::exists(fs::path("cookie")))
        fail("The cleanup part was not executed even though the test case set "
             "has.cleanup to true");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__kill_children);
ATF_TEST_CASE_BODY(run_test_case__kill_children)
{
    atf_helper helper(this, "spawn_blocking_child");
    helper.set_config("control_dir", fs::current_path());
    ATF_REQUIRE(engine::test_result(engine::test_result::passed) ==
                helper.run());

    if (!fs::exists(fs::path("pid")))
        fail("The pid file was not created");
    std::ifstream pidfile("pid");
    ATF_REQUIRE(pidfile);
    pid_t pid;
    pidfile >> pid;
    pidfile.close();

    int attempts = 30;
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
            ::usleep(100000);
            goto retry;
        }
        fail(F("The subprocess %s of our child was not killed") % pid);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__isolation);
ATF_TEST_CASE_BODY(run_test_case__isolation)
{
    atf_helper helper(this, "validate_isolation");
    // Simple checks to make sure that isolate_process has been called.
    utils::setenv("HOME", "foobar");
    utils::setenv("LANG", "C");
    ATF_REQUIRE(engine::test_result(engine::test_result::passed) ==
                helper.run());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__allowed_architectures);
ATF_TEST_CASE_BODY(run_test_case__allowed_architectures)
{
    atf_helper helper(this, "create_cookie_in_control_dir");
    helper.set_metadata("allowed_architectures", "i386 x86_64");
    helper.config().set_string("architecture", "powerpc");
    helper.config().set_string("platform", "");
    ATF_REQUIRE(engine::test_result(engine::test_result::skipped, "Current "
                                    "architecture 'powerpc' not supported") ==
                helper.run());

    if (fs::exists(fs::path("cookie")))
        fail("The test case was not really skipped when the requirements "
             "check failed");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__allowed_platforms);
ATF_TEST_CASE_BODY(run_test_case__allowed_platforms)
{
    atf_helper helper(this, "create_cookie_in_control_dir");
    helper.set_metadata("allowed_platforms", "i386 amd64");
    helper.config().set_string("architecture", "");
    helper.config().set_string("platform", "macppc");
    ATF_REQUIRE(engine::test_result(engine::test_result::skipped, "Current "
                                    "platform 'macppc' not supported") ==
                helper.run());

    if (fs::exists(fs::path("cookie")))
        fail("The test case was not really skipped when the requirements "
             "check failed");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__required_configs);
ATF_TEST_CASE_BODY(run_test_case__required_configs)
{
    atf_helper helper(this, "create_cookie_in_control_dir");
    helper.set_metadata("required_configs", "used-var");
    helper.set_config("control_dir", fs::current_path());
    helper.set_config("unused-var", "value");
    ATF_REQUIRE(engine::test_result(engine::test_result::skipped, "Required "
                                    "configuration property 'used-var' not "
                                    "defined") ==
                helper.run());

    if (fs::exists(fs::path("cookie")))
        fail("The test case was not really skipped when the requirements "
             "check failed");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__required_programs);
ATF_TEST_CASE_BODY(run_test_case__required_programs)
{
    atf_helper helper(this, "create_cookie_in_control_dir");
    helper.set_metadata("required_programs", "/non-existent/program");
    ATF_REQUIRE(engine::test_result(engine::test_result::skipped, "Required "
                                    "program '/non-existent/program' not "
                                    "found") ==
                helper.run());

    if (fs::exists(fs::path("cookie")))
        fail("The test case was not really skipped when the requirements "
             "check failed");
}


ATF_TEST_CASE(run_test_case__required_user__root__ok);
ATF_TEST_CASE_HEAD(run_test_case__required_user__root__ok)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(run_test_case__required_user__root__ok)
{
    atf_helper helper(this, "create_cookie_in_workdir");
    helper.set_metadata("required_user", "root");
    ATF_REQUIRE(passwd::current_user().is_root());
    ATF_REQUIRE(engine::test_result(engine::test_result::passed) ==
                helper.run());
}


ATF_TEST_CASE(run_test_case__required_user__root__skip);
ATF_TEST_CASE_HEAD(run_test_case__required_user__root__skip)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(run_test_case__required_user__root__skip)
{
    atf_helper helper(this, "create_cookie_in_workdir");
    helper.set_metadata("required_user", "root");
    ATF_REQUIRE(!passwd::current_user().is_root());
    ATF_REQUIRE(engine::test_result(engine::test_result::skipped, "Requires "
                                    "root privileges") ==
                helper.run());
}


ATF_TEST_CASE(run_test_case__required_user__unprivileged__ok);
ATF_TEST_CASE_HEAD(run_test_case__required_user__unprivileged__ok)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(run_test_case__required_user__unprivileged__ok)
{
    atf_helper helper(this, "create_cookie_in_workdir");
    helper.set_metadata("required_user", "unprivileged");
    ATF_REQUIRE(!helper.config().is_set("unprivileged_user"));
    ATF_REQUIRE(engine::test_result(engine::test_result::passed) ==
                helper.run());
}


ATF_TEST_CASE(run_test_case__required_user__unprivileged__skip);
ATF_TEST_CASE_HEAD(run_test_case__required_user__unprivileged__skip)
{
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(run_test_case__required_user__unprivileged__skip)
{
    atf_helper helper(this, "create_cookie_in_workdir");
    helper.set_metadata("required_user", "unprivileged");
    ATF_REQUIRE(!helper.config().is_set("unprivileged_user"));
    ATF_REQUIRE(engine::test_result(engine::test_result::skipped, "Requires "
                                    "an unprivileged user but the "
                                    "unprivileged-user configuration variable "
                                    "is not defined") ==
                helper.run());
}


ATF_TEST_CASE(run_test_case__required_user__unprivileged__drop);
ATF_TEST_CASE_HEAD(run_test_case__required_user__unprivileged__drop)
{
    set_md_var("require.config", "unprivileged-user");
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(run_test_case__required_user__unprivileged__drop)
{
    atf_helper helper(this, "check_unprivileged");
    helper.set_metadata("required_user", "unprivileged");
    helper.config().set< user_files::user_node >(
        "unprivileged_user",
        passwd::find_user_by_name(get_config_var("unprivileged-user")));
    ATF_REQUIRE(engine::test_result(engine::test_result::passed) ==
                helper.run());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__timeout_body);
ATF_TEST_CASE_BODY(run_test_case__timeout_body)
{
    atf_helper helper(this, "timeout_body");
    helper.set_metadata("timeout", "1");
    helper.set_config("control_dir", fs::current_path());
    ATF_REQUIRE(engine::test_result(engine::test_result::broken,
                                    "Test case body timed out") ==
                helper.run());

    if (fs::exists(fs::path("cookie")))
        fail("It seems that the test case was not killed after it timed out");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__timeout_cleanup);
ATF_TEST_CASE_BODY(run_test_case__timeout_cleanup)
{
    atf_helper helper(this, "timeout_cleanup");
    helper.set_metadata("has_cleanup", "true");
    helper.set_metadata("timeout", "1");
    helper.set_config("control_dir", fs::current_path());
    ATF_REQUIRE(engine::test_result(engine::test_result::broken,
                                    "Test case cleanup timed out") ==
                helper.run());

    if (fs::exists(fs::path("cookie")))
        fail("It seems that the test case was not killed after it timed out");
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__stacktrace__body);
ATF_TEST_CASE_BODY(run_test_case__stacktrace__body)
{
    atf_helper helper(this, "crash");
    capture_hooks hooks;
    const engine::test_result result = helper.run(hooks);
    ATF_REQUIRE(engine::test_result::broken == result.type());
    ATF_REQUIRE_MATCH("received signal.*core dumped", result.reason());

    ATF_REQUIRE(!atf::utils::grep_collection("attempting to gather stack trace",
                                             hooks.stdout_lines));
    ATF_REQUIRE( atf::utils::grep_collection("attempting to gather stack trace",
                                             hooks.stderr_lines));
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__stacktrace__cleanup);
ATF_TEST_CASE_BODY(run_test_case__stacktrace__cleanup)
{
    atf_helper helper(this, "crash_cleanup");
    helper.set_metadata("has_cleanup", "true");
    capture_hooks hooks;
    const engine::test_result result = helper.run(hooks);
    ATF_REQUIRE(engine::test_result::broken == result.type());
    ATF_REQUIRE_MATCH("cleanup did not terminate successfully",
                      result.reason());

    ATF_REQUIRE(!atf::utils::grep_collection("attempting to gather stack trace",
                                             hooks.stdout_lines));
    ATF_REQUIRE( atf::utils::grep_collection("attempting to gather stack trace",
                                             hooks.stderr_lines));
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__missing_results_file);
ATF_TEST_CASE_BODY(run_test_case__missing_results_file)
{
    atf_helper helper(this, "crash");
    const engine::test_result result = helper.run();
    ATF_REQUIRE(engine::test_result::broken == result.type());
    // Need to match instead of doing an explicit comparison because the string
    // may include the "core dumped" substring.
    ATF_REQUIRE_MATCH(F("Premature exit: received signal %s") % SIGABRT,
                      result.reason());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__missing_test_program);
ATF_TEST_CASE_BODY(run_test_case__missing_test_program)
{
    atf_helper helper(this, "crash");
    ATF_REQUIRE(::mkdir("dir", 0755) != -1);
    helper.move("runner_helpers", "dir");
    ATF_REQUIRE(::unlink("dir/runner_helpers") != -1);
    const engine::test_result result = helper.run();
    ATF_REQUIRE(engine::test_result::broken == result.type());
    ATF_REQUIRE_MATCH("Failed to execute", result.reason());
}


ATF_TEST_CASE_WITHOUT_HEAD(run_test_case__output);
ATF_TEST_CASE_BODY(run_test_case__output)
{
    atf_helper helper(this, "output");
    helper.set_metadata("has_cleanup", "true");

    capture_hooks hooks;
    ATF_REQUIRE(engine::test_result(engine::test_result::passed) ==
                helper.run(hooks));

    std::vector< std::string > expout;
    expout.push_back("Body message to stdout");
    expout.push_back("Cleanup message to stdout");
    ATF_REQUIRE(hooks.stdout_lines == expout);

    std::vector< std::string > experr;
    experr.push_back("Body message to stderr");
    experr.push_back("Cleanup message to stderr");
    ATF_REQUIRE(hooks.stderr_lines == experr);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, run_test_case__current_directory);
    ATF_ADD_TEST_CASE(tcs, run_test_case__subdirectory);
    ATF_ADD_TEST_CASE(tcs, run_test_case__config_variables);
    ATF_ADD_TEST_CASE(tcs, run_test_case__cleanup_shares_workdir);
    ATF_ADD_TEST_CASE(tcs, run_test_case__has_cleanup__false);
    ATF_ADD_TEST_CASE(tcs, run_test_case__has_cleanup__true);
    ATF_ADD_TEST_CASE(tcs, run_test_case__kill_children);
    ATF_ADD_TEST_CASE(tcs, run_test_case__isolation);
    ATF_ADD_TEST_CASE(tcs, run_test_case__allowed_architectures);
    ATF_ADD_TEST_CASE(tcs, run_test_case__allowed_platforms);
    ATF_ADD_TEST_CASE(tcs, run_test_case__required_configs);
    ATF_ADD_TEST_CASE(tcs, run_test_case__required_programs);
    ATF_ADD_TEST_CASE(tcs, run_test_case__required_user__root__ok);
    ATF_ADD_TEST_CASE(tcs, run_test_case__required_user__root__skip);
    ATF_ADD_TEST_CASE(tcs, run_test_case__required_user__unprivileged__ok);
    ATF_ADD_TEST_CASE(tcs, run_test_case__required_user__unprivileged__skip);
    ATF_ADD_TEST_CASE(tcs, run_test_case__required_user__unprivileged__drop);
    ATF_ADD_TEST_CASE(tcs, run_test_case__timeout_body);
    ATF_ADD_TEST_CASE(tcs, run_test_case__timeout_cleanup);
    ATF_ADD_TEST_CASE(tcs, run_test_case__stacktrace__body);
    ATF_ADD_TEST_CASE(tcs, run_test_case__stacktrace__cleanup);
    ATF_ADD_TEST_CASE(tcs, run_test_case__missing_results_file);
    ATF_ADD_TEST_CASE(tcs, run_test_case__missing_test_program);
    ATF_ADD_TEST_CASE(tcs, run_test_case__output);
}
