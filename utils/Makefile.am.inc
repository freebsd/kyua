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

noinst_LIBRARIES += libutils.a
libutils_a_SOURCES  = utils/auto_array.hpp
libutils_a_SOURCES += utils/auto_array.ipp
libutils_a_SOURCES += utils/defs.hpp
libutils_a_SOURCES += utils/env.hpp
libutils_a_SOURCES += utils/env.cpp
libutils_a_SOURCES += utils/noncopyable.hpp
libutils_a_SOURCES += utils/optional.hpp
libutils_a_SOURCES += utils/optional.ipp
libutils_a_SOURCES += utils/sanity.cpp
libutils_a_SOURCES += utils/sanity.hpp

noinst_LIBRARIES += libutils_test.a
libutils_test_a_SOURCES  = utils/test_utils.cpp
libutils_test_a_SOURCES += utils/test_utils.hpp
libutils_test_a_CXXFLAGS = $(ATF_CXX_CFLAGS)
libutils_test_a_LIBADD = libutils.a

tests_utilsdir = $(pkgtestsdir)/utils

tests_utils_DATA = utils/Atffile utils/kyua.suite
EXTRA_DIST += $(tests_utils_DATA)

tests_utils_PROGRAMS = utils/auto_array_test
utils_auto_array_test_SOURCES = utils/auto_array_test.cpp
utils_auto_array_test_CXXFLAGS = $(ATF_CXX_CFLAGS)
utils_auto_array_test_LDADD = libutils.a $(ATF_CXX_LIBS)

tests_utils_PROGRAMS += utils/env_test
utils_env_test_SOURCES = utils/env_test.cpp
utils_env_test_CXXFLAGS = $(ATF_CXX_CFLAGS)
utils_env_test_LDADD = libutils.a $(ATF_CXX_LIBS)

tests_utils_PROGRAMS += utils/optional_test
utils_optional_test_SOURCES = utils/optional_test.cpp
utils_optional_test_CXXFLAGS = $(ATF_CXX_CFLAGS)
utils_optional_test_LDADD = libutils.a $(ATF_CXX_LIBS)

tests_utils_PROGRAMS += utils/sanity_test
utils_sanity_test_SOURCES = utils/sanity_test.cpp
utils_sanity_test_CXXFLAGS = $(ATF_CXX_CFLAGS)
utils_sanity_test_LDADD = libutils_test.a libutils.a $(ATF_CXX_LIBS)

include utils/cmdline/Makefile.am.inc
include utils/format/Makefile.am.inc
include utils/fs/Makefile.am.inc
include utils/process/Makefile.am.inc
