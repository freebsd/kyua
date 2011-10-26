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

STORE_CFLAGS = $(UTILS_CFLAGS)
STORE_LIBS = libstore.a $(UTILS_LIBS)

noinst_LIBRARIES += libstore.a
libstore_a_CPPFLAGS = -DKYUA_STOREDIR=\"$(storedir)\"
libstore_a_SOURCES  = store/backend.cpp
libstore_a_SOURCES += store/backend.hpp
libstore_a_SOURCES += store/exceptions.cpp
libstore_a_SOURCES += store/exceptions.hpp
libstore_a_SOURCES += store/metadata.cpp
libstore_a_SOURCES += store/metadata.hpp

storedir = $(pkgdatadir)/store
dist_store_DATA = store/schema.sql

tests_storedir = $(pkgtestsdir)/store

tests_store_DATA = store/Atffile store/Kyuafile
EXTRA_DIST += $(tests_store_DATA)

tests_store_PROGRAMS = store/backend_test
store_backend_test_SOURCES = store/backend_test.cpp
store_backend_test_CXXFLAGS = $(STORE_CFLAGS) $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
store_backend_test_LDADD = $(STORE_LIBS) $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_store_PROGRAMS += store/exceptions_test
store_exceptions_test_SOURCES = store/exceptions_test.cpp
store_exceptions_test_CXXFLAGS = $(STORE_CFLAGS) $(UTILS_CFLAGS) \
                                 $(ATF_CXX_CFLAGS)
store_exceptions_test_LDADD = $(STORE_LIBS) $(UTILS_LIBS) \
                              $(ATF_CXX_LIBS)

tests_store_PROGRAMS += store/metadata_test
store_metadata_test_SOURCES = store/metadata_test.cpp
store_metadata_test_CXXFLAGS = $(STORE_CFLAGS) $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
store_metadata_test_LDADD = $(STORE_LIBS) $(UTILS_LIBS) $(ATF_CXX_LIBS)
