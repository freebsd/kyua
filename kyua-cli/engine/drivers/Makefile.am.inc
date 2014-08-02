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

libengine_a_SOURCES += engine/drivers/debug_test.cpp
libengine_a_SOURCES += engine/drivers/debug_test.hpp
libengine_a_SOURCES += engine/drivers/list_tests.cpp
libengine_a_SOURCES += engine/drivers/list_tests.hpp
libengine_a_SOURCES += engine/drivers/run_tests.cpp
libengine_a_SOURCES += engine/drivers/run_tests.hpp
libengine_a_SOURCES += engine/drivers/scan_results.cpp
libengine_a_SOURCES += engine/drivers/scan_results.hpp

if WITH_ATF
tests_engine_driversdir = $(pkgtestsdir)/engine/drivers

tests_engine_drivers_DATA = engine/drivers/Kyuafile
EXTRA_DIST += $(tests_engine_drivers_DATA)

tests_engine_drivers_PROGRAMS = engine/drivers/list_tests_helpers
engine_drivers_list_tests_helpers_SOURCES = \
    engine/drivers/list_tests_helpers.cpp
engine_drivers_list_tests_helpers_CXXFLAGS = $(ATF_CXX_CFLAGS)
engine_drivers_list_tests_helpers_LDADD = $(ATF_CXX_LIBS)

tests_engine_drivers_PROGRAMS += engine/drivers/list_tests_test
engine_drivers_list_tests_test_SOURCES = engine/drivers/list_tests_test.cpp
engine_drivers_list_tests_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                                          $(ATF_CXX_CFLAGS)
engine_drivers_list_tests_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) \
                                       $(ATF_CXX_LIBS)

tests_engine_drivers_PROGRAMS += engine/drivers/scan_results_test
engine_drivers_scan_results_test_SOURCES = engine/drivers/scan_results_test.cpp
engine_drivers_scan_results_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                                            $(ATF_CXX_CFLAGS)
engine_drivers_scan_results_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) \
                                         $(ATF_CXX_LIBS)
endif
