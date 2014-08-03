# Copyright 2011 Google Inc.
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

BUILD_MANPAGE = \
	test -d doc || mkdir -p doc; \
	sed -e 's,__CONFDIR__,$(kyua_confdir),g' \
	    -e 's,__DOCDIR__,$(docdir),g' \
	    -e 's,__EGDIR__,$(examplesdir),g' \
	    -e 's,__MISCDIR__,$(miscdir),g' \
	    -e 's,__PACKAGE__,$(PACKAGE_TARNAME),g' \
	    -e 's,__STOREDIR__,$(storedir),g' \
	    -e 's,__TESTSDIR__,$(testsdir),g' \
	    -e 's,__TESTERSDIR__,$(testersdir),g' \
	    -e 's,__VERSION__,$(PACKAGE_VERSION),g' \
	    <$(srcdir)/doc/$${name}.in >doc/$${name}.tmp; \
	mv doc/$${name}.tmp doc/$${name}

man_MANS = doc/kyua-about.1
CLEANFILES += doc/kyua-about.1
EXTRA_DIST += doc/kyua-about.1.in
doc/kyua-about.1: $(srcdir)/doc/kyua-about.1.in
	name=kyua-about.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua-build-root.7
CLEANFILES += doc/kyua-build-root.7
EXTRA_DIST += doc/kyua-build-root.7.in
doc/kyua-build-root.7: $(srcdir)/doc/kyua-build-root.7.in
	name=kyua-build-root.7; $(BUILD_MANPAGE)

man_MANS += doc/kyua-config.1
CLEANFILES += doc/kyua-config.1
EXTRA_DIST += doc/kyua-config.1.in
doc/kyua-config.1: $(srcdir)/doc/kyua-config.1.in
	name=kyua-config.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua-db-exec.1
CLEANFILES += doc/kyua-db-exec.1
EXTRA_DIST += doc/kyua-db-exec.1.in
doc/kyua-db-exec.1: $(srcdir)/doc/kyua-db-exec.1.in
	name=kyua-db-exec.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua-db-migrate.1
CLEANFILES += doc/kyua-db-migrate.1
EXTRA_DIST += doc/kyua-db-migrate.1.in
doc/kyua-db-migrate.1: $(srcdir)/doc/kyua-db-migrate.1.in
	name=kyua-db-migrate.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua-debug.1
CLEANFILES += doc/kyua-debug.1
EXTRA_DIST += doc/kyua-debug.1.in
doc/kyua-debug.1: $(srcdir)/doc/kyua-debug.1.in
	name=kyua-debug.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua-help.1
CLEANFILES += doc/kyua-help.1
EXTRA_DIST += doc/kyua-help.1.in
doc/kyua-help.1: $(srcdir)/doc/kyua-help.1.in
	name=kyua-help.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua-list.1
CLEANFILES += doc/kyua-list.1
EXTRA_DIST += doc/kyua-list.1.in
doc/kyua-list.1: $(srcdir)/doc/kyua-list.1.in
	name=kyua-list.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua-report-html.1
CLEANFILES += doc/kyua-report-html.1
EXTRA_DIST += doc/kyua-report-html.1.in
doc/kyua-report-html.1: $(srcdir)/doc/kyua-report-html.1.in
	name=kyua-report-html.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua-report-junit.1
CLEANFILES += doc/kyua-report-junit.1
EXTRA_DIST += doc/kyua-report-junit.1.in
doc/kyua-report-junit.1: $(srcdir)/doc/kyua-report-junit.1.in
	name=kyua-report-junit.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua-report.1
CLEANFILES += doc/kyua-report.1
EXTRA_DIST += doc/kyua-report.1.in
doc/kyua-report.1: $(srcdir)/doc/kyua-report.1.in
	name=kyua-report.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua-results-files.7
CLEANFILES += doc/kyua-results-files.7
EXTRA_DIST += doc/kyua-results-files.7.in
doc/kyua-results-files.7: $(srcdir)/doc/kyua-results-files.7.in
	name=kyua-results-files.7; $(BUILD_MANPAGE)

man_MANS += doc/kyua-test.1
CLEANFILES += doc/kyua-test.1
EXTRA_DIST += doc/kyua-test.1.in
doc/kyua-test.1: $(srcdir)/doc/kyua-test.1.in
	name=kyua-test.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua-test-filters.7
CLEANFILES += doc/kyua-test-filters.7
EXTRA_DIST += doc/kyua-test-filters.7.in
doc/kyua-test-filters.7: $(srcdir)/doc/kyua-test-filters.7.in
	name=kyua-test-filters.7; $(BUILD_MANPAGE)

man_MANS += doc/kyua.1
CLEANFILES += doc/kyua.1
EXTRA_DIST += doc/kyua.1.in
doc/kyua.1: $(srcdir)/doc/kyua.1.in
	name=kyua.1; $(BUILD_MANPAGE)

man_MANS += doc/kyua.conf.5
CLEANFILES += doc/kyua.conf.5
EXTRA_DIST += doc/kyua.conf.5.in
doc/kyua.conf.5: $(srcdir)/doc/kyua.conf.5.in
	name=kyua.conf.5; $(BUILD_MANPAGE)

man_MANS += doc/kyuafile.5
CLEANFILES += doc/kyuafile.5
EXTRA_DIST += doc/kyuafile.5.in
doc/kyuafile.5: $(srcdir)/doc/kyuafile.5.in
	name=kyuafile.5; $(BUILD_MANPAGE)
