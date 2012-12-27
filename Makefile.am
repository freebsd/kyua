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

ACLOCAL_AMFLAGS = -I m4

PHONY_TARGETS =

doc_DATA = AUTHORS COPYING NEWS README
noinst_DATA = INSTALL README
EXTRA_DIST = $(doc_DATA) INSTALL README

noinst_LIBRARIES = libtester.a
libtester_a_CPPFLAGS  = -DGDB=\"$(GDB)\"
libtester_a_CPPFLAGS += -DKYUA_TMPDIR=\"$(KYUA_TMPDIR)\"
libtester_a_SOURCES  = cli.c
libtester_a_SOURCES += cli.h
libtester_a_SOURCES += cli_fwd.h
libtester_a_SOURCES += env.c
libtester_a_SOURCES += env.h
libtester_a_SOURCES += error.c
libtester_a_SOURCES += error.h
libtester_a_SOURCES += error_fwd.h
libtester_a_SOURCES += fs.c
libtester_a_SOURCES += fs.h
libtester_a_SOURCES += result.c
libtester_a_SOURCES += result.h
libtester_a_SOURCES += run.c
libtester_a_SOURCES += run.h
libtester_a_SOURCES += run_fwd.h
libtester_a_SOURCES += stacktrace.c
libtester_a_SOURCES += stacktrace.h
libtester_a_SOURCES += text.c
libtester_a_SOURCES += text.h

noinst_LIBRARIES += libatf_tester.a
libatf_tester_a_SOURCES  = atf_list.c
libatf_tester_a_SOURCES += atf_list.h
libatf_tester_a_SOURCES += atf_result.c
libatf_tester_a_SOURCES += atf_result.h

libexec_PROGRAMS = kyua-atf-tester
kyua_atf_tester_SOURCES = atf_main.c
kyua_atf_tester_LDADD = libatf_tester.a libtester.a

libexec_PROGRAMS += kyua-plain-tester
kyua_plain_tester_SOURCES = plain_main.c
kyua_plain_tester_LDADD = libtester.a

BUILD_MANPAGE = \
	sed -e 's,__PACKAGE__,$(PACKAGE_TARNAME),g' \
	    -e 's,__TMPDIR__,$(KYUA_TMPDIR),g' \
	    <$(srcdir)/$${name}.in >$${name}.tmp; \
	mv $${name}.tmp $${name}

man_MANS  = kyua-atf-interface.7
man_MANS += kyua-plain-interface.7
man_MANS += kyua-tester-list.5
man_MANS += kyua-tester-result.5

man_MANS += kyua-atf-tester.1
CLEANFILES = kyua-atf-tester.1
EXTRA_DIST += kyua-atf-tester.1.in
kyua-atf-tester.1: $(srcdir)/kyua-atf-tester.1.in
	name=kyua-atf-tester.1; $(BUILD_MANPAGE)

man_MANS += kyua-plain-tester.1
CLEANFILES += kyua-plain-tester.1
EXTRA_DIST += kyua-plain-tester.1.in
kyua-plain-tester.1: $(srcdir)/kyua-plain-tester.1.in
	name=kyua-plain-tester.1; $(BUILD_MANPAGE)

man_MANS += kyua-tester.1
CLEANFILES += kyua-tester.1
EXTRA_DIST += kyua-tester.1.in
kyua-tester.1: $(srcdir)/kyua-tester.1.in
	name=kyua-tester.1; $(BUILD_MANPAGE)

pkgconfig_DATA = kyua-testers.pc
CLEANFILES += kyua-testers.pc
EXTRA_DIST += kyua-testers.pc.in
kyua-testers.pc: $(srcdir)/kyua-testers.pc.in Makefile
	sed -e 's#__LIBEXECDIR__#$(libexecdir)#g' \
	    -e 's#__VERSION__#$(PACKAGE_VERSION)#g' \
	    <$(srcdir)/kyua-testers.pc.in >kyua-testers.pc.tmp
	mv kyua-testers.pc.tmp kyua-testers.pc

if WITH_ATF
pkgtests_DATA = Kyuafile

pkgtests_PROGRAMS = atf_list_test
atf_list_test_SOURCES = atf_list_test.c
atf_list_test_CPPFLAGS = $(ATF_C_CFLAGS)
atf_list_test_LDADD = libatf_tester.a libtester.a $(ATF_C_LIBS)

pkgtests_PROGRAMS += atf_result_test
atf_result_test_SOURCES = atf_result_test.c
atf_result_test_CPPFLAGS = $(ATF_C_CFLAGS)
atf_result_test_LDADD = libatf_tester.a libtester.a $(ATF_C_LIBS)

pkgtests_PROGRAMS += atf_helpers
atf_helpers_SOURCES = atf_helpers.c
atf_helpers_CPPFLAGS = $(ATF_C_CFLAGS)
atf_helpers_LDADD = $(ATF_C_LIBS)

pkgtests_PROGRAMS += atf_inttest
atf_inttest_SOURCES = atf_inttest.c common_inttest.h
atf_inttest_CPPFLAGS  = -DTESTERSDIR=\"$(libexecdir)\" $(ATF_C_CFLAGS)
atf_inttest_LDADD = $(ATF_C_LIBS)

pkgtests_PROGRAMS += cli_test
cli_test_SOURCES = cli_test.c
cli_test_CPPFLAGS = $(ATF_C_CFLAGS)
cli_test_LDADD = libtester.a $(ATF_C_LIBS)

pkgtests_PROGRAMS += env_test
env_test_SOURCES = env_test.c
env_test_CPPFLAGS = $(ATF_C_CFLAGS)
env_test_LDADD = libtester.a $(ATF_C_LIBS)

pkgtests_PROGRAMS += error_test
error_test_SOURCES = error_test.c
error_test_CPPFLAGS = $(ATF_C_CFLAGS)
error_test_LDADD = libtester.a $(ATF_C_LIBS)

pkgtests_PROGRAMS += fs_test
fs_test_SOURCES = fs_test.c
fs_test_CPPFLAGS = $(ATF_C_CFLAGS)
fs_test_LDADD = libtester.a $(ATF_C_LIBS)

pkgtests_PROGRAMS += plain_inttest
plain_inttest_SOURCES = plain_inttest.c common_inttest.h
plain_inttest_CPPFLAGS  = -DTESTERSDIR=\"$(libexecdir)\" $(ATF_C_CFLAGS)
plain_inttest_LDADD = $(ATF_C_LIBS)

pkgtests_PROGRAMS += plain_helpers
plain_helpers_SOURCES = plain_helpers.c

pkgtests_PROGRAMS += result_test
result_test_SOURCES = result_test.c
result_test_CPPFLAGS = $(ATF_C_CFLAGS)
result_test_LDADD = libtester.a $(ATF_C_LIBS)

pkgtests_PROGRAMS += run_test
run_test_SOURCES = run_test.c
run_test_CPPFLAGS = $(ATF_C_CFLAGS)
run_test_LDADD = libtester.a $(ATF_C_LIBS)

pkgtests_PROGRAMS += stacktrace_helper
stacktrace_helper_SOURCES = stacktrace_helper.c

pkgtests_PROGRAMS += stacktrace_test
stacktrace_test_SOURCES = stacktrace_test.c
stacktrace_test_CPPFLAGS = $(ATF_C_CFLAGS)
stacktrace_test_LDADD = libtester.a $(ATF_C_LIBS)

pkgtests_PROGRAMS += text_test
text_test_SOURCES = text_test.c
text_test_CPPFLAGS = $(ATF_C_CFLAGS)
text_test_LDADD = libtester.a $(ATF_C_LIBS)
endif

CHECK_ENVIRONMENT  = PATH=$(abs_top_builddir):$${PATH}
CHECK_ENVIRONMENT += TESTERSDIR=$(abs_top_builddir)
INSTALLCHECK_ENVIRONMENT  = KYUA_CONFDIR=/non-existent
INSTALLCHECK_ENVIRONMENT += PATH=$(prefix)/bin:$${PATH}

if WITH_ATF
check-local: check-kyua
PHONY_TARGETS += check-kyua
check-kyua:
	$(CHECK_ENVIRONMENT) $(TESTS_ENVIRONMENT) \
	    $(KYUA) --config=none test \
            --kyuafile='$(top_srcdir)/Kyuafile' --build-root='$(top_builddir)'

installcheck-local: installcheck-kyua
PHONY_TARGETS += installcheck-kyua
installcheck-kyua:
	cd $(pkgtestsdir) && $(INSTALLCHECK_ENVIRONMENT) $(TESTS_ENVIRONMENT) \
	    $(KYUA) --config=none test
else
dist-hook: forbid-dist
PHONY_TARGETS += forbid-dist
forbid-dist:
	@echo "Sorry; cannot make dist without atf."
	@false
endif

if WITH_DOXYGEN
# This should probably be html-local, but it seems better to generate the
# documentation in all cases to get warnings about missing documentation every
# time the code is modified.  (And, after all, the documentation is not
# installed so generating it unconditionally is not a big problem.)
all-local: api-docs/api-docs.tag

api-docs/api-docs.tag: $(builddir)/Doxyfile $(SOURCES)
	mkdir -p api-docs
	${DOXYGEN} $(builddir)/Doxyfile 2>&1 | tee api-docs/warnings

clean-local: clean-api-docs
clean-api-docs:
	rm -rf api-docs
endif

PHONY_TARGETS += clean-all
clean-all:
	GIT="$(GIT)" $(SH) $(srcdir)/admin/clean-all.sh

.PHONY: $(PHONY_TARGETS)
