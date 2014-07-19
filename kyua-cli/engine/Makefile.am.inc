# Copyright 2010 Google Inc.
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

ENGINE_CFLAGS = $(STORE_CFLAGS) $(UTILS_CFLAGS)
ENGINE_LIBS = libengine.a $(STORE_LIBS) $(UTILS_LIBS)

noinst_LIBRARIES += libengine.a
libengine_a_CPPFLAGS  = $(UTILS_CFLAGS)
libengine_a_CPPFLAGS += -DKYUA_TESTERSDIR="\"$(testersdir)\""
libengine_a_SOURCES  = engine/config.cpp
libengine_a_SOURCES += engine/config.hpp
libengine_a_SOURCES += engine/context.cpp
libengine_a_SOURCES += engine/context.hpp
libengine_a_SOURCES += engine/exceptions.cpp
libengine_a_SOURCES += engine/exceptions.hpp
libengine_a_SOURCES += engine/filters.cpp
libengine_a_SOURCES += engine/filters.hpp
libengine_a_SOURCES += engine/kyuafile.cpp
libengine_a_SOURCES += engine/kyuafile.hpp
libengine_a_SOURCES += engine/metadata.cpp
libengine_a_SOURCES += engine/metadata.hpp
libengine_a_SOURCES += engine/report_junit.cpp
libengine_a_SOURCES += engine/report_junit.hpp
libengine_a_SOURCES += engine/test_case.cpp
libengine_a_SOURCES += engine/test_case.hpp
libengine_a_SOURCES += engine/test_program.cpp
libengine_a_SOURCES += engine/test_program.hpp
libengine_a_SOURCES += engine/test_result.cpp
libengine_a_SOURCES += engine/test_result.hpp
libengine_a_SOURCES += engine/testers.cpp
libengine_a_SOURCES += engine/testers.hpp

if WITH_ATF
tests_enginedir = $(pkgtestsdir)/engine

tests_engine_DATA = engine/Kyuafile
EXTRA_DIST += $(tests_engine_DATA)

tests_engine_PROGRAMS = engine/config_test
engine_config_test_SOURCES = engine/config_test.cpp
engine_config_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
engine_config_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/context_test
engine_context_test_SOURCES = engine/context_test.cpp
engine_context_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                               $(ATF_CXX_CFLAGS)
engine_context_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/exceptions_test
engine_exceptions_test_SOURCES = engine/exceptions_test.cpp
engine_exceptions_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                                  $(ATF_CXX_CFLAGS)
engine_exceptions_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) \
                               $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/filters_test
engine_filters_test_SOURCES = engine/filters_test.cpp
engine_filters_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                               $(ATF_CXX_CFLAGS)
engine_filters_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/kyuafile_test
engine_kyuafile_test_SOURCES = engine/kyuafile_test.cpp
engine_kyuafile_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                                $(ATF_CXX_CFLAGS)
engine_kyuafile_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/metadata_test
engine_metadata_test_SOURCES = engine/metadata_test.cpp
engine_metadata_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_TEST_CFLAGS) \
                                $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
engine_metadata_test_LDADD = $(ENGINE_LIBS) $(UTILS_TEST_LIBS) $(UTILS_LIBS) \
                             $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/report_junit_test
engine_report_junit_test_SOURCES = engine/report_junit_test.cpp
engine_report_junit_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                                    $(ATF_CXX_CFLAGS)
engine_report_junit_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) \
                                 $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/test_case_atf_helpers
engine_test_case_atf_helpers_SOURCES = engine/test_case_atf_helpers.cpp
engine_test_case_atf_helpers_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
engine_test_case_atf_helpers_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/test_case_plain_helpers
engine_test_case_plain_helpers_SOURCES = engine/test_case_plain_helpers.cpp
engine_test_case_plain_helpers_CXXFLAGS = $(UTILS_CFLAGS)
engine_test_case_plain_helpers_LDADD = $(UTILS_LIBS)

tests_engine_PROGRAMS += engine/test_case_test
engine_test_case_test_SOURCES = engine/test_case_test.cpp
engine_test_case_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                                 $(ATF_CXX_CFLAGS)
engine_test_case_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) \
                              $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/test_program_test
engine_test_program_test_SOURCES = engine/test_program_test.cpp
engine_test_program_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                                    $(ATF_CXX_CFLAGS)
engine_test_program_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) \
                                 $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/test_result_test
engine_test_result_test_SOURCES = engine/test_result_test.cpp
engine_test_result_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                                   $(ATF_CXX_CFLAGS)
engine_test_result_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) \
                                $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/testers_test
engine_testers_test_SOURCES = engine/testers_test.cpp
engine_testers_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                               $(ATF_CXX_CFLAGS)
engine_testers_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) $(ATF_CXX_LIBS)
endif

include engine/drivers/Makefile.am.inc
