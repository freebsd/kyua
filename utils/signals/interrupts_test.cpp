// Copyright 2012 The Kyua Authors.
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

#include "utils/signals/interrupts.hpp"

extern "C" {
#include <sys/wait.h>

#include <signal.h>
#include <unistd.h>
}

#include <cstdlib>
#include <iostream>

#include <atf-c++.hpp>

#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/process/child.ipp"
#include "utils/process/status.hpp"
#include "utils/signals/exceptions.hpp"
#include "utils/signals/programmer.hpp"

namespace fs = utils::fs;
namespace process = utils::process;
namespace signals = utils::signals;


namespace {


/// Child process that pauses waiting to be killed.
static void
pause_child(void)
{
    sigset_t mask;
    sigemptyset(&mask);
    // We loop waiting for signals because we want the parent process to send us
    // a SIGKILL that we cannot handle, not just any non-deadly signal.
    for (;;) {
        std::cerr << F("Waiting for any signal; pid=%s\n") % ::getpid();
        ::sigsuspend(&mask);
        std::cerr << F("Signal received; pid=%s\n") % ::getpid();
    }
}


/// Checks that interrupts handling manages a particular signal.
///
/// \param signo The signal to check.
static void
check_signal_handling(const int signo)
{
    const pid_t pid = ::fork();
    ATF_REQUIRE(pid != -1);
    if (pid == 0) {
        try {
            signals::setup_interrupts();

            signals::check_interrupt();  // Should not throw.

            std::cout << "Sending first interrupt; should not cause death\n";
            ::kill(::getpid(), signo);
            std::cout << "OK, first interrupt didn't terminate us\n";
            atf::utils::create_file("interrupted.txt", "");

            try {
                // Signals are caught in a different thread that may not run
                // immediately after we send the signal above.  Wait for a bit
                // if that's the case.
                int max_tries = 10;
                while (max_tries > 0) {
                    signals::check_interrupt();
                    std::cerr << "Interrupt still not detected; waiting\n";
                    ::sleep(1);
                    --max_tries;
                }
                std::cerr << "Second check_interrupt didn't know about the "
                    "interrupt; failing\n";
                std::exit(EXIT_FAILURE);
            } catch (signals::interrupted_error e) {
                // Should still throw; OK.
            }

            try {
                signals::check_interrupt();
            } catch (signals::interrupted_error e) {
                std::cerr << "Third check_interrupt still detected signal;"
                    "cleanup logic cannot run this way\n";
                throw e;
            }

            // Send us a second interrupt, which will cause an abrupt
            // termination.
            std::cout << "Sending second interrupt; should cause death\n";
            ::kill(::getpid(), signo);
            ::sleep(60);  // Long enough for the handler to run.
            std::cout << "Oops, second interrupt didn't terminate us\n";

            // Not reached.  Exit gracefully to let the parent process know.
            std::exit(EXIT_SUCCESS);
        } catch (...) {
            std::cerr << "Caught unexpected exception in child\n";
            std::exit(EXIT_FAILURE);
        }
    }

    int status;
    ATF_REQUIRE(::waitpid(pid, &status, 0) != -1);
    ATF_REQUIRE(WIFSIGNALED(status));
    ATF_REQUIRE_EQ(signo, WTERMSIG(status));

    // If the cookie does not exist, the first signal delivery caused the
    // process to incorrectly exit.
    ATF_REQUIRE(fs::exists(fs::path("interrupted.txt")));
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(sighup);
ATF_TEST_CASE_BODY(sighup)
{
    check_signal_handling(SIGHUP);
}


ATF_TEST_CASE_WITHOUT_HEAD(sigint);
ATF_TEST_CASE_BODY(sigint)
{
    check_signal_handling(SIGINT);
}


ATF_TEST_CASE_WITHOUT_HEAD(sigterm);
ATF_TEST_CASE_BODY(sigterm)
{
    check_signal_handling(SIGTERM);
}


ATF_TEST_CASE(kill_children);
ATF_TEST_CASE_HEAD(kill_children)
{
    set_md_var("timeout", "10");
}
ATF_TEST_CASE_BODY(kill_children)
{
    std::auto_ptr< process::child > child1(process::child::fork_files(
         pause_child, fs::path("/dev/stdout"), fs::path("/dev/stderr")));
    std::auto_ptr< process::child > child2(process::child::fork_files(
         pause_child, fs::path("/dev/stdout"), fs::path("/dev/stderr")));

    signals::setup_interrupts();

    // Our children pause until the reception of a signal.  Interrupting
    // ourselves will cause the signal to be re-delivered to our children due to
    // the interrupts_handler semantics.  If this does not happen, the wait
    // calls below would block indefinitely and cause our test to time out.
    ::kill(::getpid(), SIGHUP);

    const process::status status1 = child1->wait();
    ATF_REQUIRE(status1.signaled());
    ATF_REQUIRE_EQ(SIGKILL, status1.termsig());
    const process::status status2 = child2->wait();
    ATF_REQUIRE(status2.signaled());
    ATF_REQUIRE_EQ(SIGKILL, status2.termsig());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, sighup);
    ATF_ADD_TEST_CASE(tcs, sigint);
    ATF_ADD_TEST_CASE(tcs, sigterm);
    ATF_ADD_TEST_CASE(tcs, kill_children);
}
