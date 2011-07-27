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
#include <sstream>

#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;

using utils::optional;


namespace {


static void
fail(const char* str)
{
    std::cerr << str << '\n';
    std::exit(EXIT_FAILURE);
}


}  // anonymous namespace


static void
test_block(void)
{
    const fs::path control_dir = fs::path(utils::getenv("CONTROL_DIR").get());

    std::ofstream cookie((control_dir / "workdir").c_str());
    cookie << fs::current_path().str() << "\n";
    cookie.close();

    int monitor_pid;
    {
        std::istringstream input(utils::getenv("MONITOR_PID").get());
        input >> monitor_pid;
    }

    int signo;
    {
        std::istringstream input(utils::getenv("SIGNO").get());
        input >> signo;
    }

    ::sleep(1);
    ::kill(monitor_pid, signo);
    for (;;)
        ::pause();
}


static void
test_create_cookie_in_workdir(void)
{
    std::ofstream file("cookie");
    if (!file)
        fail("Failed to create the cookie");
    file.close();
}


static void
test_crash(void)
{
    std::abort();
}


static void
test_fail(void)
{
    std::exit(8);
}


static void
test_pass(void)
{
}


static void
test_spawn_blocking_child(void)
{
    pid_t pid = ::fork();
    if (pid == -1)
        fail("Cannot fork subprocess");
    else if (pid == 0) {
        for (;;)
            ::pause();
    } else {
        const fs::path name = fs::path(utils::getenv("CONTROL_DIR").get()) /
            "pid";
        std::ofstream pidfile(name.c_str());
        if (!pidfile)
            fail("Failed to create the pidfile");
        pidfile << pid;
        pidfile.close();
    }
}


static void
test_timeout(void)
{
    ::sleep(10);
    const fs::path control_dir = fs::path(utils::getenv("CONTROL_DIR").get());
    std::ofstream file((control_dir / "cookie").c_str());
    if (!file)
        fail("Failed to create the control cookie");
    file.close();
}


static void
test_validate_env(void)
{
    if (utils::getenv("HOME").get() != fs::current_path().str())
        fail("HOME not reset");
    if (utils::getenv("LANG"))
        fail("LANG not unset");
    if (utils::getenv("LC_ALL"))
        fail("LC_ALL not unset");
    if (utils::getenv("LC_COLLATE"))
        fail("LC_COLLATE not unset");
    if (utils::getenv("LC_CTYPE"))
        fail("LC_CTYPE not unset");
    if (utils::getenv("LC_MESSAGES"))
        fail("LC_MESSAGES not unset");
    if (utils::getenv("LC_MONETARY"))
        fail("LC_MONETARY not unset");
    if (utils::getenv("LC_NUMERIC"))
        fail("LC_NUMERIC not unset");
    if (utils::getenv("LC_TIME"))
        fail("LC_TIME not unset");
    if (utils::getenv("TZ").get() != "UTC")
        fail("TZ not set to UTC");
}


static void
test_validate_pgrp(void)
{
    if (::getpgrp() != ::getpid())
        fail("Test case not running in its own process group");
}


static void
test_validate_signal(void)
{
    std::istringstream iss(utils::getenv("SIGNO").get());
    int signo;
    iss >> signo;
    std::cout << "Delivering signal " << signo << "\n";
    ::kill(::getpid(), signo);
}


static void
test_validate_timezone(void)
{
    const datetime::timestamp fake = datetime::timestamp::from_values(
        2011, 5, 13, 12, 20, 30);
    if ("2011-05-13 12:20:30" != fake.strftime("%Y-%m-%d %H:%M:%S"))
        fail("Invalid defaut TZ");
}


static void
test_validate_umask(void)
{
    const mode_t old_umask = ::umask(0111);
    if (old_umask != 0022)
        fail("umask not set to 0022 when running test case");
}


int
main(int argc, char* argv[])
{
    if (argc != 1) {
        std::cerr << "No arguments allowed; select the test case with the "
            "TEST_CASE variable";
        return EXIT_FAILURE;
    }

    const optional< std::string > test_case_env = utils::getenv("TEST_CASE");
    if (!test_case_env) {
        std::cerr << "TEST_CASE not defined";
        return EXIT_FAILURE;
    }
    const std::string& test_case = test_case_env.get();

    if (test_case == "block")
        test_block();
    else if (test_case == "create_cookie_in_workdir")
        test_create_cookie_in_workdir();
    else if (test_case == "crash")
        test_crash();
    else if (test_case == "fail")
        test_fail();
    else if (test_case == "pass")
        test_pass();
    else if (test_case == "spawn_blocking_child")
        test_spawn_blocking_child();
    else if (test_case == "timeout")
        test_timeout();
    else if (test_case == "validate_env")
        test_validate_env();
    else if (test_case == "validate_pgrp")
        test_validate_pgrp();
    else if (test_case == "validate_signal")
        test_validate_signal();
    else if (test_case == "validate_timezone")
        test_validate_timezone();
    else if (test_case == "validate_umask")
        test_validate_umask();
    else {
        std::cerr << "Unknown test case";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
