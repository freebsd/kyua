// Copyright 2015 The Kyua Authors.
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

#include "utils/process/deadline_killer.hpp"

#include <chrono>
#include <map>
#include <mutex>
#include <set>
#include <thread>

#include "utils/datetime.hpp"
#include "utils/logging/macros.hpp"
#include "utils/process/operations.hpp"
#include "utils/sanity.hpp"

namespace datetime = utils::datetime;
namespace process = utils::process;


namespace {


/// Ordered collection of PIDs by the time they have to be killed.
typedef std::multimap< datetime::timestamp, int > pids_by_deadline_map;

/// Global mutex to protect static fields.
static std::mutex mutex;

/// True if the killer thread has been started.  The thread is detached and left
/// running so this never becomes false again.
static bool started = false;

/// PIDs that have deadline_killer objects alive ordered by their deadline.
static pids_by_deadline_map pids_by_deadline;


/// Calculates the PIDs whose deadline has expired.
///
/// This collects the matching PIDs from pids_by_deadline and removes them from
/// the global set.
///
/// \return A collection of PIDs.
static std::set< int >
extract_pids_to_kill(void)
{
    std::set< int > pids_to_kill;

    std::lock_guard< std::mutex > lock(mutex);
    const datetime::timestamp now = datetime::timestamp::now();
    auto iter = pids_by_deadline.begin();
    while (iter != pids_by_deadline.end() && iter->first <= now) {
        pids_to_kill.insert(iter->second);

        auto previous = iter;
        ++iter;
        pids_by_deadline.erase(previous);
    }

    return pids_to_kill;
}


/// Thread that kills PIDs with expired deadlines periodically.
static void
killer_thread(void)
{
    for (;;) {
        const std::set< int > pids_to_kill = extract_pids_to_kill();
        for (auto pid : pids_to_kill) {
            process::terminate_group(pid);
        }

        // TODO(jmmv): Instead of sleeping in a loop perpetually when there are
        // no instances of deadline_killer left behind, we could block until a
        // new one is created... or we could even shut the thread down.  Unclear
        // if these "improvements" are worthwhile because this class is used to
        // control the execution of all tests and, thorough the lifetime of a
        // single Kyua run, there is a lot of churn in deadline_killer
        // creations.  The overhead of controlling when or when not to sleep
        // could be worse than the once-a-second wakeups.
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}


}  // anonymous namespace


/// Constructor.
///
/// \param delta Time to the timer activation.
/// \param pid PID of the process (and process group) to kill.
process::deadline_killer::deadline_killer(const datetime::delta& delta,
                                          const int pid) :
    _pid(pid)
{
    std::lock_guard< std::mutex > lock(mutex);
    const datetime::timestamp now = datetime::timestamp::now();
    pids_by_deadline.insert(pids_by_deadline_map::value_type(now + delta, pid));
    if (!started) {
        std::thread thread(killer_thread);
        thread.detach();
        started = true;
    }

    _scheduled = true;
}


/// Destructor; unschedules the PID's death if still alive.
///
/// Given that this is a destructor and it can't report errors back to the
/// caller, the caller must attempt to call unschedule() on its own.
process::deadline_killer::~deadline_killer(void)
{
    if (_scheduled) {
        LW("Destroying still-scheduled process::deadline_killer object");
        try {
            unschedule();
        } catch (const std::exception& e) {
            UNREACHABLE;
        }
    }
}


/// Unschedules the PID's death.
///
/// This can only be called once.
///
/// \return True if the process was killed because its deadline expired; false
/// otherwise.
bool
process::deadline_killer::unschedule(void)
{
    PRE(_scheduled);

    std::lock_guard< std::mutex > lock(mutex);
    bool found = false;
    for (auto iter = pids_by_deadline.begin(); iter != pids_by_deadline.end();
         ++iter) {
        if (iter->second == _pid) {
            pids_by_deadline.erase(iter);
            found = true;
            break;
        }
    }

    _scheduled = false;

    return !found;
}
