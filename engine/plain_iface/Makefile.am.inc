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

libengine_a_SOURCES += engine/plain_iface/test_case.cpp
libengine_a_SOURCES += engine/plain_iface/test_case.hpp
libengine_a_SOURCES += engine/plain_iface/test_program.cpp
libengine_a_SOURCES += engine/plain_iface/test_program.hpp

if WITH_ATF
tests_engine_plain_ifacedir = $(pkgtestsdir)/engine/plain_iface

tests_engine_plain_iface_DATA = engine/plain_iface/Kyuafile
EXTRA_DIST += $(tests_engine_plain_iface_DATA)

tests_engine_plain_iface_PROGRAMS = engine/plain_iface/test_case_helpers
engine_plain_iface_test_case_helpers_SOURCES = \
    engine/plain_iface/test_case_helpers.cpp
engine_plain_iface_test_case_helpers_CXXFLAGS = $(UTILS_CFLAGS)
engine_plain_iface_test_case_helpers_LDADD = $(UTILS_LIBS)

tests_engine_plain_iface_PROGRAMS += engine/plain_iface/test_case_test
engine_plain_iface_test_case_test_SOURCES = \
    engine/plain_iface/test_case_test.cpp
engine_plain_iface_test_case_test_CXXFLAGS = $(ENGINE_CFLAGS) \
                                             $(UTILS_TEST_CFLAGS) \
                                             $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
engine_plain_iface_test_case_test_LDADD = $(ENGINE_LIBS) $(UTILS_TEST_LIBS) \
                                          $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_engine_plain_iface_PROGRAMS += engine/plain_iface/test_program_test
engine_plain_iface_test_program_test_SOURCES = \
    engine/plain_iface/test_program_test.cpp
engine_plain_iface_test_program_test_CXXFLAGS = $(ENGINE_CFLAGS) \
                                                $(UTILS_TEST_CFLAGS) \
                                                $(UTILS_CFLAGS) \
                                                $(ATF_CXX_CFLAGS)
engine_plain_iface_test_program_test_LDADD = $(ENGINE_LIBS) $(UTILS_TEST_LIBS) \
                                             $(UTILS_LIBS) $(ATF_CXX_LIBS)
endif
