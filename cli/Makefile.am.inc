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

noinst_LIBRARIES += libcli.a
libcli_a_SOURCES  = cli/all_commands.cpp
libcli_a_SOURCES += cli/all_commands.hpp
libcli_a_SOURCES += cli/cmd_about.cpp
libcli_a_SOURCES += cli/cmd_about.hpp
libcli_a_SOURCES += cli/cmd_help.cpp
libcli_a_SOURCES += cli/cmd_help.hpp
libcli_a_SOURCES += cli/cmd_list.cpp
libcli_a_SOURCES += cli/cmd_list.hpp
libcli_a_SOURCES += cli/cmd_test.cpp
libcli_a_SOURCES += cli/cmd_test.hpp
libcli_a_SOURCES += cli/main.cpp
libcli_a_SOURCES += cli/main.hpp
libcli_a_CPPFLAGS = -DKYUA_DOCDIR="\"$(docdir)\""
libcli_a_LIBADD = libutils.a

tests_clidir = $(pkgtestsdir)/cli

tests_cli_DATA = cli/Atffile cli/kyua.suite
EXTRA_DIST += $(tests_cli_DATA)

CLI_TESTS_LDADD = libcli.a libengine.a libutils_test.a libutils.a \
                  $(ATF_CXX_LIBS)

tests_cli_PROGRAMS = cli/cmd_about_test
cli_cmd_about_test_SOURCES = cli/cmd_about_test.cpp
cli_cmd_about_test_CXXFLAGS = $(ATF_CXX_CFLAGS)
cli_cmd_about_test_LDADD = $(CLI_TESTS_LDADD)

tests_cli_PROGRAMS += cli/cmd_help_test
cli_cmd_help_test_SOURCES = cli/cmd_help_test.cpp
cli_cmd_help_test_CXXFLAGS = $(ATF_CXX_CFLAGS)
cli_cmd_help_test_LDADD = $(CLI_TESTS_LDADD)

tests_cli_PROGRAMS += cli/main_test
cli_main_test_SOURCES = cli/main_test.cpp
cli_main_test_CXXFLAGS = $(ATF_CXX_CFLAGS)
cli_main_test_LDADD = $(CLI_TESTS_LDADD)
