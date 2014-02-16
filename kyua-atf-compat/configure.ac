dnl Copyright 2012 Google Inc.
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

AC_INIT([Kyua - Compatibility with ATF tools], [0.2],
        [kyua-discuss@googlegroups.com], [kyua-atf-compat],
        [https://github.com/jmmv/kyua/])
AC_PREREQ([2.65])


AC_COPYRIGHT([Copyright 2012 Google Inc.])
AC_CONFIG_AUX_DIR([admin])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_SRCDIR([atf2kyua.sh])


AM_INIT_AUTOMAKE([1.9 check-news foreign subdir-objects -Wall])


AC_PATH_PROG([KYUA], [kyua])
AC_PATH_PROG([SH], [bash ksh sh])


m4_ifndef([PKG_CHECK_MODULES],
    [m4_fatal([Cannot find pkg.m4; see the INSTALL document for help])])

m4_ifndef([ATF_CHECK_SH],
    [m4_fatal([Cannot find atf-sh.m4; see the INSTALL document for help])])
ATF_CHECK_SH([>= 0.15])
m4_ifndef([ATF_ARG_WITH],
    [m4_fatal([Cannot find atf-common.m4; see the INSTALL document for help])])
ATF_ARG_WITH


AC_ARG_VAR([ATF_CONFSUBDIR],
           [Subdirectory of sysconfdir under which to look for files])
if test x"${ATF_CONFSUBDIR-unset}" = x"unset"; then
    ATF_CONFSUBDIR=atf
else
    case ${ATF_CONFSUBDIR} in
    /*)
        AC_MSG_ERROR([ATF_CONFSUBDIR must hold a relative path])
        ;;
    *)
        ;;
    esac
fi
if test x"${ATF_CONFSUBDIR}" = x""; then
    AC_SUBST(atf_confdir, \${sysconfdir})
else
    AC_SUBST(atf_confdir, \${sysconfdir}/${ATF_CONFSUBDIR})
fi


AC_SUBST(pkgtestsdir, \${testsdir}/kyua-atf-compat)
AC_SUBST(testsdir, \${exec_prefix}/tests)


AC_OUTPUT
