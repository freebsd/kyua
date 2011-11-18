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

extern "C" {
#include <sys/stat.h>

#include <signal.h>
#include <unistd.h>
}

#include <cstdlib>
#include <fstream>
#include <iostream>

#include <atf-c++.hpp>

#include "engine/isolation.ipp"
#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/process/children.hpp"
#include "utils/sanity.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace process = utils::process;

using utils::optional;


namespace {


void
fork_and_wait_hook_ok(void)
{
    std::cout << "stdout message\n";
    std::cerr << "stderr message\n";
    std::exit(32);
}


void
fork_and_wait_hook_block(void)
{
    ::pause();
}


void
isolate_process_check_pgrp(void)
{
    engine::isolate_process(fs::path("workdir"));
    std::exit(::getpid() == ::getpgrp() ? EXIT_SUCCESS : EXIT_FAILURE);
}


template< int Signo >
void
isolate_process_kill_self(void)
{
    engine::isolate_process(fs::path("workdir"));
    ::kill(::getpid(), Signo);
    std::exit(EXIT_SUCCESS);
}


class protected_run_hook_simple {
    const engine::test_result _result;

public:
    protected_run_hook_simple(const engine::test_result& result_) :
        _result(result_)
    {
    }

    const engine::test_result&
    operator()(const fs::path& workdir)
    {
        ATF_REQUIRE_EQ(fs::path("my-tmpdir"), workdir.branch_path());
        return _result;
    }
};


class protected_run_hook_protect {
    const engine::test_result _result;

public:
    protected_run_hook_protect(const engine::test_result& result_) :
        _result(result_)
    {
    }

    const engine::test_result&
    operator()(const fs::path& workdir)
    {
        ATF_REQUIRE_EQ(fs::path("my-tmpdir"), workdir.branch_path());
        ::chmod(workdir.branch_path().c_str(), 0555);
        return _result;
    }
};


class protected_run_hook_signal {
    const int _signo;

public:
    protected_run_hook_signal(const int signo_) :
        _signo(signo_)
    {
    }

    engine::test_result
    operator()(const fs::path& workdir)
    {
        ATF_REQUIRE_EQ(fs::path("my-tmpdir"), workdir.branch_path());
        ::kill(::getpid(), _signo);
        return engine::test_result(engine::test_result::passed);
    }
};


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(create_work_directory__hardcoded);
ATF_TEST_CASE_BODY(create_work_directory__hardcoded)
{
    utils::unsetenv("TMPDIR");
    const fs::path workdir(engine::detail::create_work_directory());
    ATF_REQUIRE(::rmdir(workdir.c_str()) != -1);
    ATF_REQUIRE_EQ(fs::path("/tmp"), workdir.branch_path());
}


ATF_TEST_CASE_WITHOUT_HEAD(create_work_directory__tmpdir);
ATF_TEST_CASE_BODY(create_work_directory__tmpdir)
{
    utils::setenv("TMPDIR", ".");
    const fs::path workdir(engine::detail::create_work_directory());
    ATF_REQUIRE(::rmdir(workdir.c_str()) != -1);
    ATF_REQUIRE_EQ(fs::path("."), workdir.branch_path());
    ATF_REQUIRE_EQ("kyua.", workdir.leaf_name().substr(0, 5));
}


ATF_TEST_CASE_WITHOUT_HEAD(fork_and_wait__ok);
ATF_TEST_CASE_BODY(fork_and_wait__ok)
{
    const optional< process::status > status = engine::fork_and_wait(
        fork_and_wait_hook_ok, fs::path("out"), fs::path("err"),
        datetime::delta(60, 0));
    ATF_REQUIRE(status);
    ATF_REQUIRE(status.get().exited());
    ATF_REQUIRE_EQ(32, status.get().exitstatus());

    {
        std::ifstream input("out");
        ATF_REQUIRE(input);
        std::string line;
        ATF_REQUIRE(std::getline(input, line).good());
        ATF_REQUIRE_EQ("stdout message", line);
    }

    {
        std::ifstream input("err");
        ATF_REQUIRE(input);
        std::string line;
        ATF_REQUIRE(std::getline(input, line).good());
        ATF_REQUIRE_EQ("stderr message", line);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(fork_and_wait__timeout);
ATF_TEST_CASE_BODY(fork_and_wait__timeout)
{
    const optional< process::status > status = engine::fork_and_wait(
        fork_and_wait_hook_block, fs::path("out"), fs::path("err"),
        datetime::delta(1, 0));
    ATF_REQUIRE(!status);
}


ATF_TEST_CASE_WITHOUT_HEAD(isolate_process__cwd);
ATF_TEST_CASE_BODY(isolate_process__cwd)
{
    ATF_REQUIRE(::mkdir("workdir", 0755) != -1);
    const fs::path exp_workdir = fs::current_path() / "workdir";

    engine::isolate_process(fs::path("workdir"));
    ATF_REQUIRE_EQ(exp_workdir, fs::current_path());
}


ATF_TEST_CASE_WITHOUT_HEAD(isolate_process__env);
ATF_TEST_CASE_BODY(isolate_process__env)
{
    utils::setenv("HOME", "foobar");
    utils::setenv("LANG", "C");
    utils::setenv("LC_ALL", "C");
    utils::setenv("LC_COLLATE", "C");
    utils::setenv("LC_CTYPE", "C");
    utils::setenv("LC_MESSAGES", "C");
    utils::setenv("LC_MONETARY", "C");
    utils::setenv("LC_NUMERIC", "C");
    utils::setenv("LC_TIME", "C");
    utils::setenv("LEAVE_ME_ALONE", "kill-some-day");
    utils::setenv("TZ", "EST+5");

    ATF_REQUIRE(::mkdir("workdir", 0755) != -1);
    engine::isolate_process(fs::path("workdir"));

    const char* to_unset[] = {"LANG", "LC_ALL", "LC_COLLATE", "LC_CTYPE",
                              "LC_MESSAGES", "LC_MONETARY", "LC_NUMERIC",
                              "LC_TIME", NULL};
    for (const char** varp = to_unset; *varp != NULL; varp++)
        if (utils::getenv(*varp))
            fail(F("%s not unset") % *varp);

    if (utils::getenv("HOME").get() != fs::current_path().str())
        fail("HOME not reset");
    if (utils::getenv("TZ").get() != "UTC")
        fail("TZ not set to UTC");

    if (utils::getenv("LEAVE_ME_ALONE").get() != "kill-some-day")
        fail("Modified environment variable that should have not been touched");
}


ATF_TEST_CASE_WITHOUT_HEAD(isolate_process__pgrp);
ATF_TEST_CASE_BODY(isolate_process__pgrp)
{
    ATF_REQUIRE(::mkdir("workdir", 0755) != -1);

    // We have to run this test through the process library because
    // isolate_process assumes that the library creates the process group.
    // Therefore, think about this as an integration test only.
    const process::status status = process::child_with_files::fork(
        isolate_process_check_pgrp, fs::path("out"), fs::path("err"))->wait();

    if (!status.exited())
        fail("Subprocess died unexpectedly");
    if (status.exitstatus() != EXIT_SUCCESS)
        fail("Subprocess did not run in a different process group");
}


ATF_TEST_CASE_WITHOUT_HEAD(isolate_process__signals);
ATF_TEST_CASE_BODY(isolate_process__signals)
{
    ATF_REQUIRE(::mkdir("workdir", 0755) != -1);

    struct ::sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    ATF_REQUIRE(::sigaction(SIGUSR2, &sa, NULL) != -1);
    ::kill(::getpid(), SIGUSR2);

    const process::status status = process::child_with_files::fork(
        isolate_process_kill_self< SIGUSR2 >,
        fs::path("out"), fs::path("err"))->wait();
    ATF_REQUIRE(status.signaled());
    ATF_REQUIRE_EQ(SIGUSR2, status.termsig());
}


ATF_TEST_CASE_WITHOUT_HEAD(isolate_process__timezone);
ATF_TEST_CASE_BODY(isolate_process__timezone)
{
    ATF_REQUIRE(::mkdir("workdir", 0755) != -1);
    engine::isolate_process(fs::path("workdir"));

    const datetime::timestamp fake = datetime::timestamp::from_values(
        2011, 5, 13, 12, 20, 30);
    if ("2011-05-13 12:20:30" != fake.strftime("%Y-%m-%d %H:%M:%S"))
        fail("Invalid defaut TZ");
}


ATF_TEST_CASE_WITHOUT_HEAD(isolate_process__umask);
ATF_TEST_CASE_BODY(isolate_process__umask)
{
    ATF_REQUIRE(::mkdir("workdir", 0755) != -1);
    engine::isolate_process(fs::path("workdir"));
    const mode_t old_umask = ::umask(0111);
    ATF_REQUIRE_EQ(0022, old_umask);
}


ATF_TEST_CASE_WITHOUT_HEAD(protected_run__ok);
ATF_TEST_CASE_BODY(protected_run__ok)
{
    ATF_REQUIRE(::mkdir("my-tmpdir", 0755) != -1);
    utils::setenv("TMPDIR", "my-tmpdir");

    const engine::test_result result(engine::test_result::skipped, "Foo");
    const protected_run_hook_simple hook(result);
    ATF_REQUIRE(result == engine::protected_run(hook));
}


ATF_TEST_CASE(protected_run__ok_but_cleanup_fail);
ATF_TEST_CASE_HEAD(protected_run__ok_but_cleanup_fail)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(protected_run__ok_but_cleanup_fail)
{
    ATF_REQUIRE(::mkdir("my-tmpdir", 0755) != -1);
    utils::setenv("TMPDIR", "my-tmpdir");

    const engine::test_result result(engine::test_result::broken, "Bar");
    const protected_run_hook_protect hook(result);
    ATF_REQUIRE(result == engine::protected_run(hook));
}


ATF_TEST_CASE(protected_run__fail_and_cleanup_fail);
ATF_TEST_CASE_HEAD(protected_run__fail_and_cleanup_fail)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(protected_run__fail_and_cleanup_fail)
{
    ATF_REQUIRE(::mkdir("my-tmpdir", 0755) != -1);
    utils::setenv("TMPDIR", "my-tmpdir");

    const engine::test_result result(engine::test_result::failed, "Oh no");
    const protected_run_hook_protect hook(result);
    ATF_REQUIRE(result == engine::protected_run(hook));
}


ATF_TEST_CASE_WITHOUT_HEAD(protected_run__interrupted);
ATF_TEST_CASE_BODY(protected_run__interrupted)
{
    const int signos[] = {SIGHUP, SIGINT, SIGTERM, -1};
    for (const int* signo = signos; *signo != -1; signo++) {
        ATF_REQUIRE(::mkdir("my-tmpdir", 0755) != -1);
        utils::setenv("TMPDIR", "my-tmpdir");

        ATF_REQUIRE_THROW(
            engine::interrupted_error,
            engine::protected_run(protected_run_hook_signal(*signo)));
        if (::rmdir("my-tmpdir") == -1)
            ATF_FAIL("Signal caught but work directory not cleaned");
    }
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, create_work_directory__hardcoded);
    ATF_ADD_TEST_CASE(tcs, create_work_directory__tmpdir);

    ATF_ADD_TEST_CASE(tcs, fork_and_wait__ok);
    ATF_ADD_TEST_CASE(tcs, fork_and_wait__timeout);

    ATF_ADD_TEST_CASE(tcs, isolate_process__cwd);
    ATF_ADD_TEST_CASE(tcs, isolate_process__env);
    ATF_ADD_TEST_CASE(tcs, isolate_process__pgrp);
    ATF_ADD_TEST_CASE(tcs, isolate_process__signals);
    ATF_ADD_TEST_CASE(tcs, isolate_process__timezone);
    ATF_ADD_TEST_CASE(tcs, isolate_process__umask);

    ATF_ADD_TEST_CASE(tcs, protected_run__ok);
    ATF_ADD_TEST_CASE(tcs, protected_run__ok_but_cleanup_fail);
    ATF_ADD_TEST_CASE(tcs, protected_run__fail_and_cleanup_fail);
    ATF_ADD_TEST_CASE(tcs, protected_run__interrupted);
}
