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

dnl -----------------------------------------------------------------------
dnl Check for C/C++ compiler flags
dnl -----------------------------------------------------------------------

dnl
dnl KYUA_CC_FLAG(flag_name, accum_var)
dnl
dnl Checks whether the C compiler supports the flag 'flag_name' and, if
dnl found, appends it to the variable 'accum_var'.
dnl
AC_DEFUN([KYUA_CC_FLAG], [
    AC_LANG_PUSH([C])
    AC_MSG_CHECKING(whether ${CC} supports $1)
    saved_cflags="${CFLAGS}"
    valid_cflag=no
    CFLAGS="${CFLAGS} $1"
    AC_LINK_IFELSE([AC_LANG_PROGRAM([], [return 0;])],
                   AC_MSG_RESULT(yes)
                   valid_cflag=yes,
                   AC_MSG_RESULT(no))
    CFLAGS="${saved_cflags}"
    AC_LANG_POP([C])

    if test ${valid_cflag} = yes; then
        $2="${$2} $1"
    fi
])

dnl
dnl KYUA_CC_FLAGS(flag_names, accum_var)
dnl Checks whether the C compiler supports the flags 'flag_names', one by
dnl one, and appends the valid ones to 'accum_var'.
dnl
AC_DEFUN([KYUA_CC_FLAGS], [
    valid_cflags=
    for f in $1; do
        KYUA_CC_FLAG(${f}, valid_cflags)
    done
    if test -n "${valid_cflags}"; then
        $2="${$2} ${valid_cflags}"
    fi
])

dnl
dnl KYUA_CXX_FLAG(flag_name, accum_var)
dnl
dnl Checks whether the C++ compiler supports the flag 'flag_name' and, if
dnl found, appends it to the variable 'accum_var'.
dnl
AC_DEFUN([KYUA_CXX_FLAG], [
    AC_LANG_PUSH([C++])
    AC_MSG_CHECKING(whether ${CXX} supports $1)
    saved_cxxflags="${CXXFLAGS}"
    valid_cxxflag=no
    CXXFLAGS="${CXXFLAGS} $1"
    AC_LINK_IFELSE([AC_LANG_PROGRAM([], [return 0;])],
                   AC_MSG_RESULT(yes)
                   valid_cxxflag=yes,
                   AC_MSG_RESULT(no))
    CXXFLAGS="${saved_cxxflags}"
    AC_LANG_POP([C++])

    if test ${valid_cxxflag} = yes; then
        $2="${$2} $1"
    fi
])

dnl
dnl KYUA_CXX_FLAGS(flag_names, accum_var)
dnl Checks whether the C++ compiler supports the flags 'flag_names', one by
dnl one, and appends the valid ones to 'accum_var'.
dnl
AC_DEFUN([KYUA_CXX_FLAGS], [
    valid_cxxflags=
    for f in $1; do
        KYUA_CXX_FLAG(${f}, valid_cxxflags)
    done
    if test -n "${valid_cxxflags}"; then
        $2="${$2} ${valid_cxxflags}"
    fi
])
