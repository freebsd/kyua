# Copyright 2010 The Kyua Authors.
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

CODE_COVERAGE_DIRS =

#
# Supporting logic to run our custom testsuite.
#

TESTS_ENVIRONMENT = PKG_CONFIG_PATH=$(prefix)/lib/pkgconfig

if HAVE_ASAN
# NB: the trailing comma is intentional. I hate automake not allowing `:=` use on subsequent lines.
if HAVE_LSAN
LSAN_OPTIONS = detect_leaks=1,
else
LSAN_OPTIONS =
endif
# Some of the testcases trigger false positives as they pass
# `SIZE_MAX - 1`, etc, to malloc(3).
ASAN_OPTIONS = ${LSAN_OPTIONS}allocator_may_return_null=1
TESTS_ENVIRONMENT+= ASAN_OPTIONS=${ASAN_OPTIONS}
endif
if HAVE_UBSAN
TESTS_ENVIRONMENT+= UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1:report_error_type=1
endif

ACLOCAL_AMFLAGS = -I m4

CHECK_BOOTSTRAP_DEPS =
CHECK_KYUA_DEPS =
CHECK_LOCAL =
CLEAN_TARGETS =
DIST_HOOKS =
PHONY_TARGETS =
CLEANFILES =

EXTRA_DIST =
noinst_DATA =
noinst_LTLIBRARIES =
noinst_SCRIPTS =

doc_DATA = AUTHORS CONTRIBUTING.md CONTRIBUTORS LICENSE NEWS
noinst_DATA += INSTALL.md README.md
EXTRA_DIST += $(doc_DATA) INSTALL.md README.md

if WITH_ATF
tests_topdir = $(pkgtestsdir)

tests_top_DATA = Kyuafile
EXTRA_DIST += $(tests_top_DATA)
endif

include admin/Makefile.am.inc
include bootstrap/Makefile.am.inc
include cli/Makefile.am.inc
include doc/Makefile.am.inc
include drivers/Makefile.am.inc
include engine/Makefile.am.inc
include examples/Makefile.am.inc
include integration/Makefile.am.inc
include misc/Makefile.am.inc
include model/Makefile.am.inc
include os/freebsd/Makefile.am.inc
include store/Makefile.am.inc
include utils/Makefile.am.inc

bin_PROGRAMS = kyua
kyua_SOURCES = main.cpp
kyua_CXXFLAGS = $(CLI_CFLAGS) $(ENGINE_CFLAGS) $(UTILS_CFLAGS)
kyua_LDADD = $(CLI_LIBS) $(ENGINE_LIBS) $(FREEBSD_LIBS) $(UTILS_LIBS)

CHECK_ENVIRONMENT = KYUA_CONFDIR="/non-existent" \
                    KYUA_DOCDIR="$(abs_top_srcdir)" \
                    KYUA_EXAMPLESDIR="$(abs_top_srcdir)/examples" \
                    KYUA_MISCDIR="$(abs_top_srcdir)/misc" \
                    KYUA_STOREDIR="$(abs_top_srcdir)/store" \
                    KYUA_STORETESTDATADIR="$(abs_top_srcdir)/store" \
                    PATH="$(abs_top_builddir):$${PATH}"
INSTALLCHECK_ENVIRONMENT = KYUA_CONFDIR="/non-existent" \
                           PATH="$(prefix)/bin:$${PATH}"

# Generate local-kyua, a wrapper shell script to run the just-built 'kyua'
# binary by pointing it to the possibly not-yet-installed data files in the
# build tree.
noinst_SCRIPTS += local-kyua
CLEANFILES += local-kyua local-kyua.tmp
local-kyua: Makefile
	$(AM_V_GEN)echo '#!/bin/sh' >local-kyua.tmp; \
	echo 'env $(CHECK_ENVIRONMENT) $(TESTS_ENVIRONMENT)' \
	    '"$(abs_top_builddir)/kyua" \
	    --config='$(KYUA_CONFIG_FILE_FOR_CHECK)' \
	    "$${@}"' >>local-kyua.tmp; \
	chmod +x local-kyua.tmp; \
	mv -f local-kyua.tmp local-kyua

if WITH_ATF
CHECK_LOCAL += dump-ulimits check-kyua
PHONY_TARGETS += check-kyua
check-kyua: $(CHECK_KYUA_DEPS)
	@failed=no; \
	./local-kyua test \
	    --kyuafile='$(top_srcdir)/Kyuafile' --build-root='$(top_builddir)' \
	    || failed=yes; \
	if [ "$${failed}" = yes ]; then \
	    ./local-kyua report --results-file='$(abs_top_srcdir)' \
	        --verbose --results-filter=broken,failed; \
	    exit 1; \
	fi

INSTALLCHECK_TARGETS =
PHONY_TARGETS += installcheck-kyua
INSTALLCHECK_TARGETS += dump-ulimits installcheck-kyua
installcheck-kyua:
	cd $(pkgtestsdir) && $(INSTALLCHECK_ENVIRONMENT) $(TESTS_ENVIRONMENT) \
	    kyua --config='$(KYUA_CONFIG_FILE_FOR_CHECK)' test; \
	ret=$$?; \
	echo $${ret}; \
	if [ -n "$$JUNIT_OUTPUT" ]; then \
	    cd $(pkgtestsdir) && $(TESTS_ENVIRONMENT) \
		    $(KYUA) --config='$(KYUA_CONFIG_FILE_FOR_CHECK)' \
		    report-junit --output="$$JUNIT_OUTPUT"; \
	    sed -i '.orig' 's/encoding="iso-8859-1"/encoding="utf-8"/' "$$JUNIT_OUTPUT"; \
	    rm -f "$$JUNIT_OUTPUT.orig"; \
	fi; \
	cd $(pkgtestsdir) && $(INSTALLCHECK_ENVIRONMENT) $(TESTS_ENVIRONMENT) \
	    kyua --config='$(KYUA_CONFIG_FILE_FOR_CHECK)' report \
	    --verbose --results-filter=broken,failed; \
	exit $${ret}

if CODE_COVERAGE
CODE_COVERAGE_EXCLUDES = '/usr/include/*' '/usr/local/include/*'
CODE_COVERAGE_OUTPUT_DIR ?= code_coverage
EXTRA_GENHTML_ARGS ?=
INSTALLCHECK_TARGETS+=	installcheck-kyua-lcov

installcheck-kyua-lcov: installcheck-kyua
	mkdir -p $(CODE_COVERAGE_OUTPUT_DIR)/html
	$(LCOV) --capture --directory $(srcdir) \
	    --output-file $(CODE_COVERAGE_OUTPUT_DIR)/lcov.info
	$(LCOV) --remove $(CODE_COVERAGE_OUTPUT_DIR)/lcov.info \
	    $(CODE_COVERAGE_EXCLUDES) \
	    --output-file $(CODE_COVERAGE_OUTPUT_DIR)/lcov-filtered.info
	$(GENHTML) -o $(CODE_COVERAGE_OUTPUT_DIR)/html $(EXTRA_GENHTML_ARGS) \
	    $(CODE_COVERAGE_OUTPUT_DIR)/lcov-filtered.info
endif

# TODO(jmmv): kyua should probably be recording this information itself as part
# of the execution context, just as we record environment variables.
PHONY_TARGETS += dump-ulimits
dump-ulimits:
	@echo "Resource limits:"
	@{ \
	    ulimit -a | sed -e 's,$$, (soft),'; \
	    ulimit -a -H | sed -e 's,$$, (hard),'; \
	} | sort | sed -e 's,^,    ,'
	@echo
else
DIST_HOOKS += dist-no-atf
PHONY_TARGETS += dist-no-atf
dist-no-atf:
	@echo "Sorry; cannot build a distfile without atf"
	@false
endif
check-local: $(CHECK_LOCAL)

if CODE_COVERAGE
PHONY_TARGETS += mostlyclean-code-coverage
mostlyclean-local: mostlyclean-code-coverage
mostlyclean-code-coverage:
	-for dir in $(CODE_COVERAGE_DIRS); do \
	    find $$dir \( -name \*.gcda -or -name \*.gcno \) -delete -print; \
	done
endif

installcheck-local: $(INSTALLCHECK_TARGETS)
if WITH_DOXYGEN
# Runs doxygen on the source tree and validates the contents of the docstrings.
# We do not do this by default, even if doxygen has been enabled, because this
# step takes a long time.  Instead, we just rely on a Travis CI build to catch
# inconsistencies.
PHONY_TARGETS += check-api-docs
check-api-docs: api-docs/api-docs.tag
	@$(AWK) -f $(srcdir)/admin/check-api-docs.awk api-docs/doxygen.out

api-docs/api-docs.tag: $(builddir)/Doxyfile $(SOURCES)
	@$(MKDIR_P) api-docs
	@rm -f api-docs/doxygen.out api-docs/doxygen.out.tmp
	$(AM_V_GEN)$(DOXYGEN) $(builddir)/Doxyfile \
	    >api-docs/doxygen.out.tmp 2>&1 && \
	    mv api-docs/doxygen.out.tmp api-docs/doxygen.out

CLEAN_TARGETS += clean-api-docs
clean-api-docs:
	rm -rf api-docs
endif

# Replace Automake's builtin check-news functionality so that we can validate
# the NEWS file instead of NEWS.
DIST_HOOKS += check-news
PHONY_TARGETS += check-news
check-news:
	@case "$$(sed 15q "$(srcdir)/NEWS")" in \
	*"$(VERSION)"*) : ;; \
	*) \
	    echo "NEWS not updated; not releasing" 1>&2; \
	    exit 1 \
	    ;; \
	esac

clean-local: $(CLEAN_TARGETS)

PHONY_TARGETS += clean-all
clean-all:
	GIT="$(GIT)" $(SH) $(srcdir)/admin/clean-all.sh

dist-hook: $(DIST_HOOKS)

.PHONY: $(PHONY_TARGETS)

# vim: syntax=make:noexpandtab:shiftwidth=8:softtabstop=8
