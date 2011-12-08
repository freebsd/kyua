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

libengine_a_SOURCES += engine/atf_iface/results.cpp
libengine_a_SOURCES += engine/atf_iface/results.hpp
libengine_a_SOURCES += engine/atf_iface/runner.cpp
libengine_a_SOURCES += engine/atf_iface/runner.hpp
libengine_a_SOURCES += engine/atf_iface/test_case.cpp
libengine_a_SOURCES += engine/atf_iface/test_case.hpp
libengine_a_SOURCES += engine/atf_iface/test_program.cpp
libengine_a_SOURCES += engine/atf_iface/test_program.hpp

tests_engine_atf_ifacedir = $(pkgtestsdir)/engine/atf_iface

tests_engine_atf_iface_DATA = engine/atf_iface/Kyuafile
EXTRA_DIST += $(tests_engine_atf_iface_DATA)

tests_engine_atf_iface_PROGRAMS = engine/atf_iface/results_test
engine_atf_iface_results_test_SOURCES = engine/atf_iface/results_test.cpp
engine_atf_iface_results_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_TEST_CFLAGS) \
                                         $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
engine_atf_iface_results_test_LDADD = $(ENGINE_LIBS) $(UTILS_TEST_LIBS) \
                                $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_engine_atf_iface_PROGRAMS += engine/atf_iface/runner_helpers
engine_atf_iface_runner_helpers_SOURCES = engine/atf_iface/runner_helpers.cpp
engine_atf_iface_runner_helpers_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
engine_atf_iface_runner_helpers_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_engine_atf_iface_PROGRAMS += engine/atf_iface/runner_test
engine_atf_iface_runner_test_SOURCES = engine/atf_iface/runner_test.cpp
engine_atf_iface_runner_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                                        $(UTILS_TEST_CFLAGS) $(ATF_CXX_CFLAGS)
engine_atf_iface_runner_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) \
                                     $(UTILS_TEST_LIBS) $(ATF_CXX_LIBS)

tests_engine_atf_iface_PROGRAMS += engine/atf_iface/test_case_test
engine_atf_iface_test_case_test_SOURCES = engine/atf_iface/test_case_test.cpp
engine_atf_iface_test_case_test_CXXFLAGS = $(ENGINE_CFLAGS) \
                                           $(UTILS_TEST_CFLAGS) \
                                           $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
engine_atf_iface_test_case_test_LDADD = $(ENGINE_LIBS) $(UTILS_TEST_LIBS) \
                                  $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_engine_atf_iface_PROGRAMS += engine/atf_iface/test_program_atf_helpers
engine_atf_iface_test_program_atf_helpers_SOURCES = \
    engine/atf_iface/test_program_atf_helpers.cpp
engine_atf_iface_test_program_atf_helpers_CXXFLAGS = $(ATF_CXX_CFLAGS)
engine_atf_iface_test_program_atf_helpers_LDADD = $(ATF_CXX_LIBS)

tests_engine_atf_iface_PROGRAMS += engine/atf_iface/test_program_plain_helpers
engine_atf_iface_test_program_plain_helpers_SOURCES = \
    engine/atf_iface/test_program_plain_helpers.cpp
engine_atf_iface_test_program_plain_helpers_CXXFLAGS = $(UTILS_CFLAGS)

tests_engine_atf_iface_PROGRAMS += engine/atf_iface/test_program_test
engine_atf_iface_test_program_test_SOURCES = \
    engine/atf_iface/test_program_test.cpp
engine_atf_iface_test_program_test_CXXFLAGS = $(ENGINE_CFLAGS) \
                                              $(UTILS_TEST_CFLAGS) \
                                              $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
engine_atf_iface_test_program_test_LDADD = $(ENGINE_LIBS) $(UTILS_TEST_LIBS) \
                                           $(UTILS_LIBS) $(ATF_CXX_LIBS)
