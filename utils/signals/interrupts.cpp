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
#include <sys/types.h>

#include <signal.h>
#include <unistd.h>
}

#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <set>
#include <thread>

#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"
#include "utils/noncopyable.hpp"
#include "utils/process/operations.hpp"
#include "utils/sanity.hpp"
#include "utils/signals/exceptions.hpp"
#include "utils/signals/misc.hpp"
#include "utils/signals/programmer.hpp"

namespace signals = utils::signals;
namespace process = utils::process;


namespace {


/// The interrupt signal that fired, or -1 if none.
static std::atomic_int fired_signal(-1);

/// Counter for the number of times our signal handler ran.
static std::atomic_int fired_signal_count(0);

/// Cause the signals thread to redeliver the signal to self to terminate.
///
/// This is set to true by signals::redeliver_to_exit() before sending the given
/// signal to self, which the signal thread later uses to forcibly terminate the
/// program.
static std::atomic_bool die_now(false);

/// Signal mask to restore after exiting a signal inhibited section.
static sigset_t global_old_sigmask;

/// Global mutex to protect the rest of the variables below.
static std::mutex mutex;

/// Set to true once the signals thread has finished setting up the handlers.
static bool started = false;

/// Set to true after the signals thread has finished killing all subprocesses.
static bool killed = false;

/// Condition variable to wait for started or killed to be set to true.
static std::condition_variable cv;

/// List of processes to kill upon reception of a signal.
static std::set< pid_t > pids_to_kill;


/// Generic handler to capture interrupt signals.
///
/// \param signo The signal that caused this handler to be called.
static void
signal_handler(const int signo)
{
    fired_signal = signo;
    fired_signal_count += 1;
}


/// Unique thread for signal handling.
///
/// This thread must be started with all signals disabled to ensure that it is
/// the only one reenabling signal handling.
///
/// The behavior of this thread is as follows: first, we await for the delivery
/// of a signal.  Once we get one, we terminate any pending processes which
/// should cause other threads to get unblocked.  Then we sleep again, waiting
/// for another signal to terminate ourselves, which can either come from Kyua's
/// main thread after catching signals::interrupted_error or from a second
/// delivery of a signal by the user.
static void
signals_handling_thread(void)
{
    signals::programmer sighup_handler(SIGHUP, signal_handler);
    signals::programmer sigint_handler(SIGINT, signal_handler);
    signals::programmer sigterm_handler(SIGTERM, signal_handler);

    ::sigset_t mask, old_mask;
    sigemptyset(&mask);
    const int ret = ::pthread_sigmask(SIG_SETMASK, &mask, &old_mask);
    INV(ret != -1);
#if !defined(NDEBUG)
    for (int signo = 1; signo < signals::last_signo; ++signo) {
        if (signo == SIGKILL || signo == SIGSTOP)
            continue;
        INV_MSG(sigismember(&old_mask, signo),
                F("Signal %s not blocked at start of thread") % signo);
    }
#endif

    {
        std::unique_lock< std::mutex > lock(mutex);
        started = true;
        cv.notify_all();
    }

    while (fired_signal == -1)
        (void)::sigsuspend(&mask);
    std::cerr << "[-- Signal caught; please wait for cleanup --]\n";

    {
        std::unique_lock< std::mutex > lock(mutex);
        for (pid_t pid : pids_to_kill) {
            process::terminate_group(pid);
        }
        killed = true;
        cv.notify_all();
    }

    while (!die_now && fired_signal_count == 1)
        (void)::sigsuspend(&mask);
    if (!die_now) {
        // Only print the message if the second signal is not because of
        // signals::redeliver_on_exit(), which would indicate a controlled exit
        // from the main thread.
        std::cerr << "[-- Double signal caught; terminating --]\n";
    }
    sigterm_handler.unprogram();
    sigint_handler.unprogram();
    sighup_handler.unprogram();
    ::kill(::getpid(), fired_signal);
}


}  // anonymous namespace


/// Checks if an interrupt has fired.
///
/// Calls to this function should be sprinkled in strategic places through the
/// code protected by an interrupts_handler object.
///
/// Only one call to this function will raise an exception per signal received.
/// This is to allow executing cleanup actions without reraising interrupt
/// exceptions unless the user has fired another interrupt.
///
/// \throw interrupted_error If there has been an interrupt.
void
signals::check_interrupt(void)
{
    const int original_fired_signal = fired_signal;
    if (original_fired_signal != -1) {
        std::unique_lock< std::mutex > lock(mutex);
        cv.wait(lock, []{return killed;});
        fired_signal = -1;
        throw interrupted_error(original_fired_signal);
    }
}


/// Registers a child process to be killed upon reception of an interrupt.
///
/// \pre The caller must ensure that the call to fork() and the addition of the
/// PID happen without interrupts checking in between.
///
/// \param pid The PID of the child process.  Must not have been yet registered.
void
signals::add_pid_to_kill(const pid_t pid)
{
    std::lock_guard< std::mutex > guard(mutex);
    PRE(pids_to_kill.find(pid) == pids_to_kill.end());
    pids_to_kill.insert(pid);
}


/// Unregisters a child process previously registered via add_pid_to_kill().
///
/// \param pid The PID of the child process.  Must have been registered
///     previously, and the process must have already been awaited for.
void
signals::remove_pid_to_kill(const pid_t pid)
{
    std::lock_guard< std::mutex > guard(mutex);
    PRE(pids_to_kill.find(pid) != pids_to_kill.end());
    pids_to_kill.erase(pid);
}


/// Starts the signals handling thread to handle interrupts asynchronously.
///
/// This configures the program to funnel all signal handling through a single
/// thread, started here.  All other threads must then check for interrupts in
/// strategic places by invoking check_interrupt().
///
/// This should be called early in the main thread, before any other threads
/// have been started, to ensure the right default signal mask is set for them.
void
signals::setup_interrupts(void)
{
    PRE(!started);

    ::sigset_t mask;
    sigfillset(&mask);
    const int ret = ::sigprocmask(SIG_BLOCK, &mask, &global_old_sigmask);
    INV(ret != -1);

    std::thread thread(signals_handling_thread);
    thread.detach();

    // Wait until the thread finishes starting up and configuring signal
    // handling.  This is necessary to avoid losing interrupts if
    // check_interrupt() is called too soon after the thread starts.
    std::unique_lock< std::mutex > lock(mutex);
    cv.wait(lock, []{return started;});
}


/// Clears interrupts handling in a new child.
///
/// This must be invoked right after fork() to ensure the child process can
/// receive signals.
void
signals::reset_interrupts_in_new_child(void)
{
    const int ret = ::sigprocmask(SIG_SETMASK, &global_old_sigmask, NULL);
    INV(ret != -1);
}


/// Redeliver a caught signal to cause the program to terminate.
///
/// This has to be invoked from the main thread once we have caught
/// interrupted_error to cause the program to terminate with the right exit
/// status.
void
signals::redeliver_to_exit(const int signo)
{
    die_now = true;
    ::kill(::getpid(), signo);
    LD("Interrupt signal re-delivery did not terminate program");
}
