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

doc_DATA = AUTHORS COPYING NEWS README
noinst_DATA = INSTALL README
EXTRA_DIST = $(doc_DATA) INSTALL README

REPLACE = echo "$${src} -> $${dst}" && \
    sed -e 's,__ATF_CONFDIR__,$(atf_confdir),g' \
        -e 's,__ATF_SH__,$(ATF_SH),g' \
        -e 's,__BINDIR__,$(bindir),g' \
        -e 's,__PKGDATADIR__,$(pkgdatadir),g' \
        -e 's,__SH__,$(SH),g' \
        $${src} >$${dst}.new && \
    chmod +x $${dst}.new && \
    mv $${dst}.new $${dst}

nodist_bin_SCRIPTS = atf-run
dist_man_MANS = atf-run.1
CLEANFILES = atf-run
EXTRA_DIST += atf-run.sh
atf-run: $(srcdir)/atf-run.sh
	@src=$(srcdir)/atf-run.sh dst=atf-run; $(REPLACE)

nodist_bin_SCRIPTS += atf-report
dist_man_MANS += atf-report.1
CLEANFILES += atf-report
EXTRA_DIST += atf-report.sh
atf-report: $(srcdir)/atf-report.sh
	@src=$(srcdir)/atf-report.sh dst=atf-report; $(REPLACE)

nodist_bin_SCRIPTS += atf2kyua
dist_man_MANS += atf2kyua.1
CLEANFILES += atf2kyua
EXTRA_DIST += atf2kyua.sh
atf2kyua: $(srcdir)/atf2kyua.sh
	@src=$(srcdir)/atf2kyua.sh dst=atf2kyua; $(REPLACE)

dist_pkgdata_DATA = lib.subr tests_lib.subr

if WITH_ATF
dist_pkgtests_DATA = Kyuafile

pkgtests_SCRIPTS = atf-run_test
CLEANFILES += atf-run_test
EXTRA_DIST += atf-run_test.sh
atf-run_test: $(srcdir)/atf-run_test.sh Makefile
	@src="$(srcdir)/atf-run_test.sh" dst="atf-run_test"; $(REPLACE)

pkgtests_SCRIPTS += atf-report_test
CLEANFILES += atf-report_test
EXTRA_DIST += atf-report_test.sh
atf-report_test: $(srcdir)/atf-report_test.sh Makefile
	@src="$(srcdir)/atf-report_test.sh" dst="atf-report_test"; $(REPLACE)

pkgtests_SCRIPTS += atf2kyua_test
CLEANFILES += atf2kyua_test
EXTRA_DIST += atf2kyua_test.sh
atf2kyua_test: $(srcdir)/atf2kyua_test.sh Makefile
	@src="$(srcdir)/atf2kyua_test.sh" dst="atf2kyua_test"; $(REPLACE)

pkgtests_SCRIPTS += helpers
CLEANFILES += helpers
EXTRA_DIST += helpers.sh
helpers: $(srcdir)/helpers.sh Makefile
	@src="$(srcdir)/helpers.sh" dst="helpers"; $(REPLACE)

CHECK_ENVIRONMENT = KYUA_ATF_COMPAT_BINDIR='$(abs_top_builddir)' \
                    KYUA_ATF_COMPAT_PKGDATADIR='$(abs_top_srcdir)' \
                    PATH=$(abs_top_builddir):$${PATH}
INSTALLCHECK_ENVIRONMENT = PATH=$(prefix)/bin:$${PATH}

check-local:
	$(CHECK_ENVIRONMENT) $(TESTS_ENVIRONMENT) \
	    $(KYUA) test --build-root='$(top_builddir)' \
            --kyuafile='$(top_srcdir)/Kyuafile'

installcheck-local:
	cd $(pkgtestsdir) && $(INSTALLCHECK_ENVIRONMENT) $(TESTS_ENVIRONMENT) \
	    $(KYUA) $(INSTALLCHECK_VARS) test
else
dist-hook:
	@echo "Sorry; cannot make dist without atf."
	@false
endif

.PHONY: clean-all
clean-all:
	GIT="$(GIT)" $(SH) $(srcdir)/admin/clean-all.sh
