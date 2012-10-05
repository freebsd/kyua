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

ENGINE_CFLAGS += $(LUTOK_CFLAGS)
ENGINE_LIBS += $(LUTOK_LIBS)

libengine_a_CPPFLAGS += $(LUTOK_CFLAGS)
libengine_a_CPPFLAGS += -DKYUA_LUADIR="\"$(luadir)\""
libengine_a_SOURCES += engine/user_files/common.cpp
libengine_a_SOURCES += engine/user_files/common.hpp
libengine_a_SOURCES += engine/user_files/config.cpp
libengine_a_SOURCES += engine/user_files/config.hpp
libengine_a_SOURCES += engine/user_files/exceptions.cpp
libengine_a_SOURCES += engine/user_files/exceptions.hpp
libengine_a_SOURCES += engine/user_files/kyuafile.cpp
libengine_a_SOURCES += engine/user_files/kyuafile.hpp

lua_modulesdir = $(luadir)
dist_lua_modules_DATA  = engine/user_files/init.lua
dist_lua_modules_DATA += engine/user_files/kyuafile_1.lua

if WITH_ATF
tests_engine_user_filesdir = $(pkgtestsdir)/engine/user_files

tests_engine_user_files_DATA = engine/user_files/Kyuafile
EXTRA_DIST += $(tests_engine_user_files_DATA)

tests_engine_user_files_PROGRAMS = engine/user_files/common_test
engine_user_files_common_test_SOURCES = engine/user_files/common_test.cpp
engine_user_files_common_test_CPPFLAGS = -DKYUA_LUADIR="\"$(luadir)\""
engine_user_files_common_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                                         $(ATF_CXX_CFLAGS)
engine_user_files_common_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) \
                                      $(ATF_CXX_LIBS)

tests_engine_user_files_PROGRAMS += engine/user_files/config_test
engine_user_files_config_test_SOURCES = engine/user_files/config_test.cpp
engine_user_files_config_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                                         $(ATF_CXX_CFLAGS)
engine_user_files_config_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) \
                                      $(ATF_CXX_LIBS)

tests_engine_user_files_PROGRAMS += engine/user_files/exceptions_test
engine_user_files_exceptions_test_SOURCES = \
    engine/user_files/exceptions_test.cpp
engine_user_files_exceptions_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                                             $(ATF_CXX_CFLAGS)
engine_user_files_exceptions_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) \
                                          $(ATF_CXX_LIBS)

tests_engine_user_files_PROGRAMS += engine/user_files/init_test
engine_user_files_init_test_SOURCES = engine/user_files/init_test.cpp
engine_user_files_init_test_CPPFLAGS = -DKYUA_LUADIR="\"$(luadir)\""
engine_user_files_init_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                                       $(ATF_CXX_CFLAGS)
engine_user_files_init_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) \
                                    $(ATF_CXX_LIBS)

tests_engine_user_files_PROGRAMS += engine/user_files/kyuafile_test
engine_user_files_kyuafile_test_SOURCES = engine/user_files/kyuafile_test.cpp
engine_user_files_kyuafile_test_CXXFLAGS = $(ENGINE_CFLAGS) \
                                           $(UTILS_CFLAGS) \
                                           $(ATF_CXX_CFLAGS)
engine_user_files_kyuafile_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) \
                                        $(ATF_CXX_LIBS)

tests_engine_user_files_PROGRAMS += engine/user_files/kyuafile_1_test
engine_user_files_kyuafile_1_test_SOURCES = \
    engine/user_files/kyuafile_1_test.cpp
engine_user_files_kyuafile_1_test_CXXFLAGS = $(ENGINE_CFLAGS) $(UTILS_CFLAGS) \
                                             $(ATF_CXX_CFLAGS)
engine_user_files_kyuafile_1_test_LDADD = $(ENGINE_LIBS) $(UTILS_LIBS) \
                                          $(ATF_CXX_LIBS)
endif
