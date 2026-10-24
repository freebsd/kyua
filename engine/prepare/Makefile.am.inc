# Copyright (c) 2024 Igor Ostapenko <igoro@FreeBSD.org>
# All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause

ENGINE_PREPARE_CFLAGS = $(LUTOK_CFLAGS)
ENGINE_PREPARE_LIBS = libengine_prepare.la $(LUTOK_LIBS)

noinst_LTLIBRARIES += libengine_prepare.la
libengine_prepare_la_CPPFLAGS = $(LUTOK_CFLAGS)
libengine_prepare_la_SOURCES  = engine/prepare/prepare.hpp
libengine_prepare_la_SOURCES += engine/prepare/prepare.cpp
libengine_prepare_la_SOURCES +=	engine/prepare/prepare_all.hpp
libengine_prepare_la_SOURCES += engine/prepare/prepare_all.cpp
