# Copyright 2010, Google Inc.
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
libengine_a_SOURCES  = engine/config.cpp
libengine_a_SOURCES += engine/config.hpp
libengine_a_SOURCES += engine/exceptions.cpp
libengine_a_SOURCES += engine/exceptions.hpp
libengine_a_SOURCES += engine/suite_config.cpp
libengine_a_SOURCES += engine/suite_config.hpp
libengine_a_SOURCES += engine/test_case.cpp
libengine_a_SOURCES += engine/test_case.hpp
libengine_a_SOURCES += engine/test_program.cpp
libengine_a_SOURCES += engine/test_program.hpp
libengine_a_SOURCES += engine/results.cpp
libengine_a_SOURCES += engine/results.hpp
libengine_a_SOURCES += engine/results.ipp
libengine_a_SOURCES += engine/runner.cpp
libengine_a_SOURCES += engine/runner.hpp

tests_enginedir = $(pkgtestsdir)/engine

tests_engine_DATA = engine/Atffile engine/kyua.suite
EXTRA_DIST += $(tests_engine_DATA)

tests_engine_PROGRAMS = engine/config_test
engine_config_test_SOURCES = engine/config_test.cpp
engine_config_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
engine_config_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/exceptions_test
engine_exceptions_test_SOURCES = engine/exceptions_test.cpp
engine_exceptions_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                                  $(ATF_CXX_CFLAGS)
engine_exceptions_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) \
                               $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/results_helpers
engine_results_helpers_SOURCES = engine/results_helpers.cpp
engine_results_helpers_CXXFLAGS = $(ATF_CXX_CFLAGS)
engine_results_helpers_LDADD = $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/results_test
engine_results_test_SOURCES = engine/results_test.cpp
engine_results_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_TEST_CFLAGS) \
                               $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
engine_results_test_LDADD = $(ENGINE_LIBS) $(UTILS_TEST_LIBS) \
                            $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/runner_helpers
engine_runner_helpers_SOURCES = engine/runner_helpers.cpp
engine_runner_helpers_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
engine_runner_helpers_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/runner_test
engine_runner_test_SOURCES = engine/runner_test.cpp
engine_runner_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
engine_runner_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/suite_config_test
engine_suite_config_test_SOURCES = engine/suite_config_test.cpp
engine_suite_config_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                                    $(ATF_CXX_CFLAGS)
engine_suite_config_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) \
                                 $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/test_case_test
engine_test_case_test_SOURCES = engine/test_case_test.cpp
engine_test_case_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_TEST_CFLAGS) \
                                 $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
engine_test_case_test_LDADD = $(ENGINE_LIBS) $(UTILS_TEST_LIBS) \
                              $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/test_program_test
engine_test_program_test_SOURCES = engine/test_program_test.cpp
engine_test_program_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                                    $(ATF_CXX_CFLAGS)
engine_test_program_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) \
                                 $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/test_program_atf_helpers
engine_test_program_atf_helpers_SOURCES = engine/test_program_atf_helpers.cpp
engine_test_program_atf_helpers_CXXFLAGS = $(ATF_CXX_CFLAGS)
engine_test_program_atf_helpers_LDADD = $(ATF_CXX_LIBS)

tests_engine_PROGRAMS += engine/test_program_plain_helpers
engine_test_program_plain_helpers_SOURCES = \
    engine/test_program_plain_helpers.cpp
