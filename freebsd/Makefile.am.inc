# Copyright 2024 The Kyua Authors.
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

FREEBSD_CFLAGS =
FREEBSD_LIBS = libfreebsd.a

noinst_LIBRARIES += libfreebsd.a
libfreebsd_a_CPPFLAGS = -DGDB=\"$(GDB)\"
libfreebsd_a_SOURCES =  freebsd/main.hpp
libfreebsd_a_SOURCES += freebsd/main.cpp
libfreebsd_a_SOURCES += freebsd/execenv_jail.hpp
libfreebsd_a_SOURCES += freebsd/execenv_jail_manager.hpp
libfreebsd_a_SOURCES += freebsd/execenv_jail_manager.cpp

if FreeBSD
FREEBSD_LIBS += -ljail
libfreebsd_a_SOURCES += freebsd/execenv_jail.cpp
include freebsd/utils/Makefile.am.inc
else
libfreebsd_a_SOURCES += freebsd/execenv_jail_stub.cpp
endif
