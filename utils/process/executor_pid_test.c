/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Dell Inc.
 * Author: Eric van Gyzen
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if 0

1. Run some "bad" tests that prevent kyua from removing the work directory.
   We use "chflags uunlink".  Mounting a file system from an md(4) device
   is another common use case.
2. Fork a lot, nearly wrapping the PID number space, so step 3 will re-use
   a PID from step 1.  Running the entire FreeBSD test suite is a more
   realistic scenario for this step.
3. Run some more tests.  If the stars align, the bug is not fixed yet, and
   kyua is built with debugging, kyua will abort with the following messages.
   Without debugging, the tests in step 3 will reuse the context from step 1,
   including stdout, stderr, and working directory, which are still populated
   with stuff from step 1.  When I found this bug, step 3 was
   __test_cases_list__, which expects a certain format in stdout and failed
   when it found something completely unrelated.
4. You can clean up with: chflags -R nouunlink /tmp/kyua.*; rm -rf /tmp/kyua.*

$ cc -o pid_wrap -latf-c pid_wrap.c
$ kyua test
pid_wrap:leak_0  ->  passed  [0.001s]
pid_wrap:leak_1  ->  passed  [0.001s]
pid_wrap:leak_2  ->  passed  [0.001s]
pid_wrap:leak_3  ->  passed  [0.001s]
pid_wrap:leak_4  ->  passed  [0.001s]
pid_wrap:leak_5  ->  passed  [0.001s]
pid_wrap:leak_6  ->  passed  [0.001s]
pid_wrap:leak_7  ->  passed  [0.001s]
pid_wrap:leak_8  ->  passed  [0.001s]
pid_wrap:leak_9  ->  passed  [0.001s]
pid_wrap:pid_wrap  ->  passed  [1.113s]
pid_wrap:pid_wrap_0  ->  passed  [0.001s]
pid_wrap:pid_wrap_1  ->  passed  [0.001s]
pid_wrap:pid_wrap_2  ->  passed  [0.001s]
pid_wrap:pid_wrap_3  ->  *** /usr/src/main/contrib/kyua/utils/process/executor.cpp:779: Invariant check failed: PID 60876 already in all_exec_handles; not properly cleaned up or reused too fast
*** Fatal signal 6 received
*** Log file is /home/vangyzen/.kyua/logs/kyua.20221006-193544.log
*** Please report this problem to kyua-discuss@googlegroups.com detailing what you were doing before the crash happened; if possible, include the log file mentioned above
Abort trap (core dumped)

#endif

#include <sys/stat.h>

#include <atf-c.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void
leak_work_dir(void)
{
	int fd;

	fd = open("unforgettable", O_CREAT|O_EXCL|O_WRONLY, 0600);
	ATF_REQUIRE_MSG(fd != -1,
	    "open(..., O_CREAT|O_EXCL|O_WRONLY, 0600) failed unexpectedly: %s",
	    strerror(errno));
	ATF_REQUIRE_EQ_MSG(0, fchflags(fd, SF_NOUNLINK),
	    "fchflags(..., UF_NOUNLINK): failed unexpectedly: %s",
	    strerror(errno));
	ATF_REQUIRE_EQ(0, close(fd));
}

void
wrap_pids(void)
{
	pid_t begin, current, target;
	bool wrapped;

	begin = getpid();
	target = begin - 15;
	if (target <= 1) {
		target += 99999;    // PID_MAX
		wrapped = true;
	} else {
		wrapped = false;
	}

	ATF_REQUIRE(signal(SIGCHLD, SIG_IGN) != SIG_ERR);

	do {
		current = vfork();
		if (current == 0) {
			_exit(0);
		}
		ATF_REQUIRE(current != -1);
		if (current < begin) {
			wrapped = true;
		}
	} while (!wrapped || current < target);
}

void
test_work_dir_reuse(void)
{
	// If kyua is built with debugging, it would abort here before the fix.
}

void
clean_up(void)
{
	(void)system("chflags -R nosunlink ../..");
}

#define	LEAK_WORKDIR_TC(name) 		\
	ATF_TC_WITHOUT_HEAD(name);	\
	ATF_TC_BODY(name, tc) { 	\
		leak_work_dir(); 	\
	}

LEAK_WORKDIR_TC(leak_0)
LEAK_WORKDIR_TC(leak_1)
LEAK_WORKDIR_TC(leak_2)
LEAK_WORKDIR_TC(leak_3)
LEAK_WORKDIR_TC(leak_4)
LEAK_WORKDIR_TC(leak_5)
LEAK_WORKDIR_TC(leak_6)
LEAK_WORKDIR_TC(leak_7)
LEAK_WORKDIR_TC(leak_8)
LEAK_WORKDIR_TC(leak_9)

ATF_TC_WITHOUT_HEAD(pid_wrap);
ATF_TC_BODY(pid_wrap, tc) { wrap_pids(); }

#define	PID_WRAP_TC(name) 		\
	ATF_TC_WITHOUT_HEAD(name);	\
	ATF_TC_BODY(name, tc) {		\
		test_work_dir_reuse();	\
	}

PID_WRAP_TC(pid_wrap_0)
PID_WRAP_TC(pid_wrap_1)
PID_WRAP_TC(pid_wrap_2)
PID_WRAP_TC(pid_wrap_3)
PID_WRAP_TC(pid_wrap_4)
PID_WRAP_TC(pid_wrap_5)
PID_WRAP_TC(pid_wrap_6)
PID_WRAP_TC(pid_wrap_7)
PID_WRAP_TC(pid_wrap_8)
PID_WRAP_TC(pid_wrap_9)

ATF_TC_WITHOUT_HEAD(really_clean_up);
ATF_TC_BODY(really_clean_up, tc)
{
	clean_up();
}

ATF_TP_ADD_TCS(tcs)
{
	ATF_TP_ADD_TC(tcs, leak_0);
	ATF_TP_ADD_TC(tcs, leak_1);
	ATF_TP_ADD_TC(tcs, leak_2);
	ATF_TP_ADD_TC(tcs, leak_3);
	ATF_TP_ADD_TC(tcs, leak_4);
	ATF_TP_ADD_TC(tcs, leak_5);
	ATF_TP_ADD_TC(tcs, leak_6);
	ATF_TP_ADD_TC(tcs, leak_7);
	ATF_TP_ADD_TC(tcs, leak_8);
	ATF_TP_ADD_TC(tcs, leak_9);

	ATF_TP_ADD_TC(tcs, pid_wrap);

	ATF_TP_ADD_TC(tcs, pid_wrap_0);
	ATF_TP_ADD_TC(tcs, pid_wrap_1);
	ATF_TP_ADD_TC(tcs, pid_wrap_2);
	ATF_TP_ADD_TC(tcs, pid_wrap_3);
	ATF_TP_ADD_TC(tcs, pid_wrap_4);
	ATF_TP_ADD_TC(tcs, pid_wrap_5);
	ATF_TP_ADD_TC(tcs, pid_wrap_6);
	ATF_TP_ADD_TC(tcs, pid_wrap_7);
	ATF_TP_ADD_TC(tcs, pid_wrap_8);
	ATF_TP_ADD_TC(tcs, pid_wrap_9);

	ATF_TP_ADD_TC(tcs, really_clean_up);

	return atf_no_error();
}
