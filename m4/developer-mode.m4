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
dnl Set up the developer mode
dnl -----------------------------------------------------------------------

dnl
dnl KYUA_DEVELOPER_MODE(languages)
dnl
dnl Adds a --enable-developer flag to the configure script and, when given,
dnl checks for and enables several flags useful during development.
dnl
AC_DEFUN([KYUA_DEVELOPER_MODE], [
    m4_foreach([language], [$1], [m4_set_add([languages], language)])

    AC_ARG_ENABLE([developer],
                  AS_HELP_STRING([--enable-developer],
                                 [enable developer features]),,
                  [case ${PACKAGE_VERSION} in
                   0.*|*99*|*alpha*|*beta*) enable_developer=yes ;;
                   *) enable_developer=no ;;
                   esac])

    if test ${enable_developer} = yes; then
        try_werror=yes

        try_c_cxx_flags="-g \
                         -Wall \
                         -Wcast-qual \
                         -Wextra \
                         -Wno-unused-parameter \
                         -Wpointer-arith \
                         -Wredundant-decls \
                         -Wreturn-type \
                         -Wshadow \
                         -Wsign-compare \
                         -Wswitch \
                         -Wwrite-strings"

        try_c_flags="-Wmissing-prototypes \
                     -Wno-traditional \
                     -Wstrict-prototypes"

        try_cxx_flags="-Wabi \
                       -Wctor-dtor-privacy \
                       -Wno-deprecated \
                       -Wno-non-template-friend \
                       -Wno-pmf-conversions \
                       -Wnon-virtual-dtor \
                       -Woverloaded-virtual \
                       -Wreorder \
                       -Wsign-promo \
                       -Wsynth"

        #
        # The following flags should also be enabled but cannot be.  Reasons
        # given below.
        #
        # -Wold-style-cast: Raises errors when using TIOCGWINSZ, at least under
        #                   Mac OS X.  This is due to the way _IOR is defined.
        #
    else
        try_werror=no
        try_c_cxx_flags="-DNDEBUG"
        try_c_flags=
        try_cxx_flags=
    fi
    try_c_cxx_flags="${try_c_cxx_flags} -D_FORTIFY_SOURCE=2"

    # Try and set -Werror first so that tests for other flags are accurate.
    # Otherwise, compilers such as clang will report the flags as a warning and
    # we will conclude they are supported... but when they are combined with
    # -Werror they cause build failures.
    if test ${try_werror} = yes; then
        m4_set_contains([languages], [C], [KYUA_CC_FLAGS(-Werror, CFLAGS)])
        m4_set_contains([languages], [C++], [KYUA_CXX_FLAGS(-Werror, CXXFLAGS)])
    fi

    m4_set_contains([languages], [C],
                    [KYUA_CC_FLAGS(${try_c_cxx_flags} ${try_c_flags},
                                   CFLAGS)])
    m4_set_contains([languages], [C++],
                    [KYUA_CXX_FLAGS(${try_c_cxx_flags} ${try_cxx_flags},
                                    CXXFLAGS)])
])
