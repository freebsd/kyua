# Copyright 2010, 2011 Google Inc.
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

ENGINE_CFLAGS = $(UTILS_CFLAGS)
ENGINE_LIBS = libengine.a $(UTILS_LIBS)

noinst_LIBRARIES += libengine.a
libengine_a_SOURCES  = engine/exceptions.cpp
libengine_a_SOURCES += engine/exceptions.hpp
libengine_a_SOURCES += engine/isolation.cpp
libengine_a_SOURCES += engine/isolation.hpp
libengine_a_SOURCES += engine/isolation.ipp
libengine_a_SOURCES += engine/test_case.cpp
libengine_a_SOURCES += engine/test_case.hpp
libengine_a_SOURCES += engine/test_program.cpp
libengine_a_SOURCES += engine/test_program.hpp
libengine_a_SOURCES += engine/results.cpp
libengine_a_SOURCES += engine/results.hpp
libengine_a_SOURCES += engine/results.ipp

tests_enginedir = $(pkgtestsdir)/engine

tests_engine_DATA = engine/Atffile engine/Kyuafile
EXTRA_DIST += $(tests_engine_DATA)

tests_engine_PROGRAMS = engine/exceptions_test
engine_exceptions_test_SOURCES = engine/exceptions_test.cpp
engine_exceptions_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                                  $(ATF_CXX_CFLAGS)
engine_exceptions_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) \
                               $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/isolation_test
engine_isolation_test_SOURCES = engine/isolation_test.cpp
engine_isolation_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_TEST_CFLAGS) \
                                 $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
engine_isolation_test_LDADD = $(ENGINE_LIBS) $(UTILS_TEST_LIBS) \
                              $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/results_test
engine_results_test_SOURCES = engine/results_test.cpp
engine_results_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_TEST_CFLAGS) \
                               $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
engine_results_test_LDADD = $(ENGINE_LIBS) $(UTILS_TEST_LIBS) \
                            $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/test_case_test
engine_test_case_test_SOURCES = engine/test_case_test.cpp
engine_test_case_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_TEST_CFLAGS) \
                                 $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
engine_test_case_test_LDADD = $(ENGINE_LIBS) $(UTILS_TEST_LIBS) \
                              $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/test_program_test
engine_test_program_test_SOURCES = engine/test_program_test.cpp
engine_test_program_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_TEST_CFLAGS) \
                                    $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
engine_test_program_test_LDADD = $(ENGINE_LIBS) $(UTILS_TEST_LIBS) \
                                 $(UTILS_LIBS) $(ATF_CXX_LIBS)

include engine/atf_iface/Makefile.am.inc
include engine/plain_iface/Makefile.am.inc
include engine/user_files/Makefile.am.inc
