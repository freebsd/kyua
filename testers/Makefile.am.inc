# Copyright 2012 Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
# * Neither the name of Google Inc. nor the names of its contributors
#   may be used to endorse or promote products derived from this software
#   without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

noinst_LIBRARIES += testers/libtester.a
testers_libtester_a_CPPFLAGS  = -DGDB=\"$(GDB)\"
testers_libtester_a_CPPFLAGS += -DKYUA_TMPDIR=\"$(KYUA_TMPDIR)\"
testers_libtester_a_SOURCES  = testers/cli.c
testers_libtester_a_SOURCES += testers/cli.h
testers_libtester_a_SOURCES += testers/cli_fwd.h
testers_libtester_a_SOURCES += testers/env.c
testers_libtester_a_SOURCES += testers/env.h
testers_libtester_a_SOURCES += testers/error.c
testers_libtester_a_SOURCES += testers/error.h
testers_libtester_a_SOURCES += testers/error_fwd.h
testers_libtester_a_SOURCES += testers/fs.c
testers_libtester_a_SOURCES += testers/fs.h
testers_libtester_a_SOURCES += testers/result.c
testers_libtester_a_SOURCES += testers/result.h
testers_libtester_a_SOURCES += testers/run.c
testers_libtester_a_SOURCES += testers/run.h
testers_libtester_a_SOURCES += testers/run_fwd.h
testers_libtester_a_SOURCES += testers/stacktrace.c
testers_libtester_a_SOURCES += testers/stacktrace.h
testers_libtester_a_SOURCES += testers/text.c
testers_libtester_a_SOURCES += testers/text.h
nodist_testers_libtester_a_SOURCES = testers/defs.h

noinst_LIBRARIES += testers/libatf_tester.a
testers_libatf_tester_a_SOURCES  = testers/atf_list.c
testers_libatf_tester_a_SOURCES += testers/atf_list.h
testers_libatf_tester_a_SOURCES += testers/atf_result.c
testers_libatf_tester_a_SOURCES += testers/atf_result.h

noinst_LIBRARIES += testers/libtap_tester.a
testers_libtap_tester_a_SOURCES  = testers/tap_parser.c
testers_libtap_tester_a_SOURCES += testers/tap_parser.h

libexec_PROGRAMS = testers/kyua-atf-tester
testers_kyua_atf_tester_SOURCES = testers/atf_main.c
testers_kyua_atf_tester_LDADD = testers/libatf_tester.a testers/libtester.a

libexec_PROGRAMS += testers/kyua-plain-tester
testers_kyua_plain_tester_SOURCES = testers/plain_main.c
testers_kyua_plain_tester_LDADD = testers/libtester.a

libexec_PROGRAMS += testers/kyua-tap-tester
testers_kyua_tap_tester_SOURCES = testers/tap_main.c
testers_kyua_tap_tester_LDADD = testers/libtap_tester.a testers/libtester.a

BUILD_TESTERS_MANPAGE = \
	sed -e 's,__PACKAGE__,$(PACKAGE_TARNAME),g' \
	    -e 's,__TMPDIR__,$(KYUA_TMPDIR),g' \
	    <$(srcdir)/$${name}.in >$${name}.tmp; \
	mv $${name}.tmp $${name}

dist_man_MANS  = testers/kyua-atf-interface.7
dist_man_MANS += testers/kyua-plain-interface.7
dist_man_MANS += testers/kyua-tap-interface.7
dist_man_MANS += testers/kyua-tester-list.5
dist_man_MANS += testers/kyua-tester-result.5

man_MANS += testers/kyua-atf-tester.1
CLEANFILES += testers/kyua-atf-tester.1
EXTRA_DIST += testers/kyua-atf-tester.1.in
testers/kyua-atf-tester.1: $(srcdir)/testers/kyua-atf-tester.1.in
	name=testers/kyua-atf-tester.1; $(BUILD_TESTERS_MANPAGE)

man_MANS += testers/kyua-plain-tester.1
CLEANFILES += testers/kyua-plain-tester.1
EXTRA_DIST += testers/kyua-plain-tester.1.in
testers/kyua-plain-tester.1: $(srcdir)/testers/kyua-plain-tester.1.in
	name=testers/kyua-plain-tester.1; $(BUILD_TESTERS_MANPAGE)

man_MANS += testers/kyua-tap-tester.1
CLEANFILES += testers/kyua-tap-tester.1
EXTRA_DIST += testers/kyua-tap-tester.1.in
testers/kyua-tap-tester.1: $(srcdir)/testers/kyua-tap-tester.1.in
	name=testers/kyua-tap-tester.1; $(BUILD_TESTERS_MANPAGE)

man_MANS += testers/kyua-tester.1
CLEANFILES += testers/kyua-tester.1
EXTRA_DIST += testers/kyua-tester.1.in
testers/kyua-tester.1: $(srcdir)/testers/kyua-tester.1.in
	name=testers/kyua-tester.1; $(BUILD_TESTERS_MANPAGE)

if WITH_ATF
tests_testersdir = $(pkgtestsdir)/testers

tests_testers_DATA = testers/Kyuafile
EXTRA_DIST += $(tests_testers_DATA)

tests_testers_PROGRAMS = testers/atf_list_test
testers_atf_list_test_SOURCES = testers/atf_list_test.c
testers_atf_list_test_CPPFLAGS = $(ATF_C_CFLAGS)
testers_atf_list_test_LDADD = testers/libatf_tester.a testers/libtester.a \
                              $(ATF_C_LIBS)

tests_testers_PROGRAMS += testers/atf_result_test
testers_atf_result_test_SOURCES = testers/atf_result_test.c
testers_atf_result_test_CPPFLAGS = $(ATF_C_CFLAGS)
testers_atf_result_test_LDADD = testers/libatf_tester.a testers/libtester.a \
                                $(ATF_C_LIBS)

tests_testers_PROGRAMS += testers/atf_helpers
testers_atf_helpers_SOURCES = testers/atf_helpers.c
testers_atf_helpers_CPPFLAGS = $(ATF_C_CFLAGS)
testers_atf_helpers_LDADD = $(ATF_C_LIBS)

tests_testers_PROGRAMS += testers/atf_inttest
testers_atf_inttest_SOURCES = testers/atf_inttest.c testers/common_inttest.h
testers_atf_inttest_CPPFLAGS = -DKYUA_TESTERSDIR=\"$(libexecdir)\" \
                               $(ATF_C_CFLAGS)
testers_atf_inttest_LDADD = $(ATF_C_LIBS)

tests_testers_PROGRAMS += testers/cli_test
testers_cli_test_SOURCES = testers/cli_test.c
testers_cli_test_CPPFLAGS = $(ATF_C_CFLAGS)
testers_cli_test_LDADD = testers/libtester.a $(ATF_C_LIBS)

tests_testers_PROGRAMS += testers/env_test
testers_env_test_SOURCES = testers/env_test.c
testers_env_test_CPPFLAGS = $(ATF_C_CFLAGS)
testers_env_test_LDADD = testers/libtester.a $(ATF_C_LIBS)

tests_testers_PROGRAMS += testers/error_test
testers_error_test_SOURCES = testers/error_test.c
testers_error_test_CPPFLAGS = $(ATF_C_CFLAGS)
testers_error_test_LDADD = testers/libtester.a $(ATF_C_LIBS)

tests_testers_PROGRAMS += testers/fs_test
testers_fs_test_SOURCES = testers/fs_test.c
testers_fs_test_CPPFLAGS = $(ATF_C_CFLAGS)
testers_fs_test_LDADD = testers/libtester.a $(ATF_C_LIBS)

tests_testers_PROGRAMS += testers/plain_inttest
testers_plain_inttest_SOURCES = testers/plain_inttest.c testers/common_inttest.h
testers_plain_inttest_CPPFLAGS = -DKYUA_TESTERSDIR=\"$(libexecdir)\" \
                                 $(ATF_C_CFLAGS)
testers_plain_inttest_LDADD = $(ATF_C_LIBS)

tests_testers_PROGRAMS += testers/plain_helpers
testers_plain_helpers_SOURCES = testers/plain_helpers.c

tests_testers_PROGRAMS += testers/result_test
testers_result_test_SOURCES = testers/result_test.c
testers_result_test_CPPFLAGS = $(ATF_C_CFLAGS)
testers_result_test_LDADD = testers/libtester.a $(ATF_C_LIBS)

tests_testers_PROGRAMS += testers/run_test
testers_run_test_SOURCES = testers/run_test.c
testers_run_test_CPPFLAGS = $(ATF_C_CFLAGS)
testers_run_test_LDADD = testers/libtester.a $(ATF_C_LIBS)

tests_testers_PROGRAMS += testers/stacktrace_helper
testers_stacktrace_helper_SOURCES = testers/stacktrace_helper.c

tests_testers_PROGRAMS += testers/stacktrace_test
testers_stacktrace_test_SOURCES = testers/stacktrace_test.c
testers_stacktrace_test_CPPFLAGS = $(ATF_C_CFLAGS)
testers_stacktrace_test_LDADD = testers/libtester.a $(ATF_C_LIBS)

tests_testers_PROGRAMS += testers/tap_inttest
testers_tap_inttest_SOURCES = testers/tap_inttest.c testers/common_inttest.h
testers_tap_inttest_CPPFLAGS = -DKYUA_TESTERSDIR=\"$(libexecdir)\" \
                               $(ATF_C_CFLAGS)
testers_tap_inttest_LDADD = $(ATF_C_LIBS)

tests_testers_PROGRAMS += testers/tap_helpers
testers_tap_helpers_SOURCES = testers/tap_helpers.c

tests_testers_PROGRAMS += testers/tap_parser_test
testers_tap_parser_test_SOURCES = testers/tap_parser_test.c
testers_tap_parser_test_CPPFLAGS = $(ATF_C_CFLAGS)
testers_tap_parser_test_LDADD = testers/libtap_tester.a testers/libtester.a \
                                $(ATF_C_LIBS)

tests_testers_PROGRAMS += testers/text_test
testers_text_test_SOURCES = testers/text_test.c
testers_text_test_CPPFLAGS = $(ATF_C_CFLAGS)
testers_text_test_LDADD = testers/libtester.a $(ATF_C_LIBS)
endif
