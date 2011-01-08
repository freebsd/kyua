# Copyright 2011, Google Inc.
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

UTILS_CFLAGS += $(LUA_CFLAGS)
UTILS_LIBS += $(LUA_LIBS)

libutils_a_CPPFLAGS += $(LUA_CFLAGS)
libutils_a_SOURCES += utils/lua/exceptions.cpp
libutils_a_SOURCES += utils/lua/exceptions.hpp
libutils_a_SOURCES += utils/lua/operations.cpp
libutils_a_SOURCES += utils/lua/operations.hpp
libutils_a_SOURCES += utils/lua/wrap.cpp
libutils_a_SOURCES += utils/lua/wrap.hpp

tests_utils_luadir = $(pkgtestsdir)/utils/lua

tests_utils_lua_DATA = utils/lua/Atffile utils/lua/kyua.suite
EXTRA_DIST += $(tests_utils_lua_DATA)

tests_utils_lua_PROGRAMS = utils/lua/exceptions_test
utils_lua_exceptions_test_SOURCES = utils/lua/exceptions_test.cpp
utils_lua_exceptions_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_lua_exceptions_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_lua_PROGRAMS += utils/lua/operations_test
utils_lua_operations_test_SOURCES = utils/lua/operations_test.cpp \
                                    utils/lua/test_utils.hpp
utils_lua_operations_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_lua_operations_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)

tests_utils_lua_PROGRAMS += utils/lua/wrap_test
utils_lua_wrap_test_SOURCES = utils/lua/test_utils.hpp \
                              utils/lua/wrap_test.cpp
utils_lua_wrap_test_CXXFLAGS = $(UTILS_CFLAGS) $(ATF_CXX_CFLAGS)
utils_lua_wrap_test_LDADD = $(UTILS_LIBS) $(ATF_CXX_LIBS)
