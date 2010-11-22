dnl Copyright 2010, Google Inc.
dnl All rights reserved.
dnl
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions are
dnl met:
dnl
dnl * Redistributions of source code must retain the above copyright
dnl   notice, this list of conditions and the following disclaimer.
dnl * Redistributions in binary form must reproduce the above copyright
dnl   notice, this list of conditions and the following disclaimer in the
dnl   documentation and/or other materials provided with the distribution.
dnl * Neither the name of Google Inc. nor the names of its contributors
dnl   may be used to endorse or promote products derived from this software
dnl   without specific prior written permission.
dnl
dnl THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
dnl "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
dnl LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
dnl A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
dnl OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
dnl SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
dnl LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
dnl DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
dnl THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
dnl (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
dnl OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

AC_INIT([Kyua - Command line interface], [0.1],
        [kyua-discuss@googlegroups.com], [kyua-cli],
        [http://code.google.com/p/kyua/])
AC_PREREQ([2.65])


AC_COPYRIGHT([Copyright 2010, Google Inc.])
AC_CONFIG_AUX_DIR([admin])
AC_CONFIG_FILES([Doxyfile Makefile utils/defs.hpp])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([main.cpp])
AC_CONFIG_TESTDIR([bootstrap])


AM_INIT_AUTOMAKE([1.9 check-news foreign subdir-objects -Wall])


AC_LANG([C++])
AC_PROG_CXX
KYUA_REQUIRE_CXX
KYUA_DEVELOPER_MODE([C++])
KYUA_ATTRIBUTE_NORETURN
KYUA_GETOPT_WITH_OPTRESET
AC_CHECK_FUNCS([putenv setenv unsetenv])


AC_MSG_CHECKING([whether getcwd(NULL, 0) works])
AC_RUN_IFELSE([AC_LANG_PROGRAM([#include <stdlib.h>
#include <unistd.h>],
    [char *cwd = getcwd(NULL, 0);
     return (cwd != NULL) ? EXIT_SUCCESS : EXIT_FAILURE;])],
    [AC_MSG_RESULT(yes)
     AC_DEFINE([HAVE_GETCWD_DYN], [1],
               [Define to 1 if getcwd(NULL, 0) works])],
    [AC_MSG_RESULT(no)])


AC_PROG_RANLIB


PKG_CHECK_MODULES([ATF_CXX], [atf-c++ >= 0.12],
                  [],
                  AC_MSG_ERROR([atf-c++ (0.12 or newer) is required]))
KYUA_DOXYGEN
AC_PATH_PROG([SVN], [svn])


dnl BSD make(1) doesn't deal with targets specified as './foo' well: they
dnl need to be specified as 'foo'.  The following hack is to workaround this
dnl issue.
if test "${srcdir}" = .; then
    target_srcdir=
else
    target_srcdir="${srcdir}/"
fi
AC_SUBST([target_srcdir])


AC_OUTPUT
