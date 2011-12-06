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
#include <sys/time.h>

#include <signal.h>
}

#include <cerrno>

#include "utils/datetime.hpp"
#include "utils/logging/macros.hpp"
#include "utils/sanity.hpp"
#include "utils/signals/exceptions.hpp"
#include "utils/signals/programmer.hpp"
#include "utils/signals/timer.hpp"


namespace {
static void sigalrm_handler(const int);
}  // anonymous namespace


namespace utils {
namespace signals {


/// Internal implementation for the signals::timer class.
struct timer::impl {
    /// Whether the timer is currently programmed or not.
    bool programmed;

    /// The timer that we replaced; to be restored on unprogramming.
    ::itimerval old_timeval;

    /// Signal programmer for SIGALRM.
    programmer sigalrm_programmer;

    /// Initializes the internal implementation of the timer.
    impl(void) :
        programmed(false),
        sigalrm_programmer(SIGALRM, ::sigalrm_handler)
    {
        // Not strictly needed but ensure that, even if we use old_timeval by
        // mistake, we do not program a timer.
        timerclear(&old_timeval.it_interval);
        timerclear(&old_timeval.it_value);
    }
};


}  // namespace signals
}  // namespace utils


namespace datetime = utils::datetime;
namespace signals = utils::signals;


namespace {


/// The function to run when SIGALRM fires.
static signals::timer_callback active_callback = NULL;


/// SIGALRM handler for the timer implementation.
///
/// \param signo The signal received; must be SIGALRM.
static void
sigalrm_handler(const int signo)
{
    PRE(signo == SIGALRM);
    PRE(active_callback != NULL);
    active_callback();
}


}  // anonymous namespace


/// Programs a timer.
///
/// The timer fires only once; intervals are not supported.
///
/// \pre There is no timer already programmed.  At the moment, this only
///     supports one single timer programmed at a time.
///
/// \param delta The time until the timer fires.
/// \param callback The function to call when the timer expires.
signals::timer::timer(const datetime::delta& delta,
                      const timer_callback callback)
{
    PRE_MSG(::active_callback == NULL, "Only one timer can be programmed at a "
            "time due to implementation limitations");

    ::active_callback = callback;
    try {
        _pimpl.reset(new impl());

        ::itimerval timeval;
        timerclear(&timeval.it_interval);
        timeval.it_value.tv_sec = delta.seconds;
        timeval.it_value.tv_usec = delta.useconds;

        if (::setitimer(ITIMER_REAL, &timeval, &_pimpl->old_timeval) == -1) {
            const int original_errno = errno;
            throw system_error("Failed to program timer", original_errno);
        } else
            _pimpl->programmed = true;
    } catch (...) {
        ::active_callback = NULL;
        throw;
    }
}


/// Destructor; unprograms the timer if still programmed.
///
/// Given that this is a destructor and it can't report errors back to the
/// caller, the caller must attempt to call unprogram() on its own.
signals::timer::~timer(void)
{
    if (_pimpl->programmed) {
        LW("Destroying still-programmed signals::timer object");
        try {
            unprogram();
        } catch (const system_error& e) {
            UNREACHABLE;
        }
    }

    ::active_callback = NULL;
}


/// Unprograms the timer.
///
/// \pre The timer is programmed (i.e. this can only be called once).
///
/// \throw system_error If unprogramming the timer failed.  If this happens,
///     the timer is left programmed, this object forgets about the timer and
///     therefore there is no way to restore the original timer.
void
signals::timer::unprogram(void)
{
    PRE(_pimpl->programmed);

    // If we fail, we don't want the destructor to attempt to unprogram the
    // handler again, as it would result in a crash.
    _pimpl->programmed = false;

    if (::setitimer(ITIMER_REAL, &_pimpl->old_timeval, NULL) == -1) {
        const int original_errno = errno;
        throw system_error("Failed to unprogram timer", original_errno);
    }

    _pimpl->sigalrm_programmer.unprogram();
}
