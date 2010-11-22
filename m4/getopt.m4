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

dnl
dnl KYUA_GETOPT_WITH_OPTRESET
dnl
dnl Checks if getopt(3) has an optreset global variable to reset internal state
dnl before calling getopt(3) again.  Defines HAVE_GETOPT_WITH_OPTRESET if
dnl optreset exists.
dnl
AC_DEFUN([KYUA_GETOPT_WITH_OPTRESET], [
    AC_MSG_CHECKING([whether getopt has optreset])

    AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <stdlib.h>
#include <unistd.h>

int
main(void)
{
    optreset = 1;
    return EXIT_SUCCESS;
}
])],
    [getopt_optreset=yes
     AC_DEFINE([HAVE_GETOPT_WITH_OPTRESET], [1],
               [Define to 1 if getopt has optreset])],
    [getopt_optreset=no])

    AC_MSG_RESULT([${getopt_optreset}])
])
