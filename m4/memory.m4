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

dnl \file m4/memory.m4
dnl
dnl Macros to configure the utils::memory module.


dnl Entry point to detect all features needed by utils::memory.
dnl
dnl This looks for a mechanism to check the available physical memory in the
dnl system.
AC_DEFUN([KYUA_MEMORY], [
    memory_query=unknown
    memory_mib=none

    _KYUA_SYSCTLBYNAME([have_sysctlbyname=yes], [have_sysctlbyname=no])
    if test "${have_sysctlbyname}" = yes; then
        for mib in hw.usermem64 hw.usermem; do
            _KYUA_SYSCTL_MIB([${mib}], [
                memory_query=sysctlbyname
                memory_mib="${mib}"
                break
            ], [])
        done
    fi

    if test "${memory_query}" = unknown; then
        AC_MSG_WARN([Don't know how to query the amount of physical memory])
        AC_MSG_WARN([The test case's require.memory property will not work])
    fi

    AC_DEFINE_UNQUOTED([MEMORY_QUERY_TYPE], ["${memory_query}"],
                       [Define to the memory query type])
    AC_DEFINE_UNQUOTED([MEMORY_QUERY_SYSCTL_MIB], ["${memory_mib}"],
                       [Define to the name of the sysctl MIB])
])


dnl Detects the availability of the sysctlbyname(3) function.
dnl
dnl \param action_if_found Code to run if the function is found.
dnl \param action_if_not_found Code to run if the function is not found.
AC_DEFUN([_KYUA_SYSCTLBYNAME], [
    AC_CHECK_HEADERS([sys/types.h sys/sysctl.h])  dnl Darwin 11.2
    AC_CHECK_HEADERS([sys/param.h sys/sysctl.h])  dnl NetBSD 6.0

    AC_CHECK_FUNCS([sysctlbyname], [$1], [$2])
])


dnl Looks for a specific sysctl MIB.
dnl
dnl \pre sysctlbyname(3) must be present in the system.
dnl
dnl \param mib_name The name of the MIB to check for.
dnl \param action_if_found Code to run if the MIB is found.
dnl \param action_if_not_found Code to run if the MIB is not found.
AC_DEFUN([_KYUA_SYSCTL_MIB], [
    AC_MSG_CHECKING([for if the $1 sysctl MIB exists])
    AC_RUN_IFELSE([AC_LANG_PROGRAM([
#if defined(HAVE_SYS_TYPES_H)
#   include <sys/types.h>
#endif
#if defined(HAVE_SYS_PARAM_H)
#   include <sys/param.h>
#endif
#if defined(HAVE_SYS_SYSCTL_H)
#   include <sys/sysctl.h>
#endif
#include <stdint.h>
#include <stdlib.h>
], [
    int64_t memory;
    size_t memory_length = sizeof(memory);
    if (sysctlbyname("$1", &memory, &memory_length, NULL, 0) == -1)
        return EXIT_FAILURE;
    else
        return EXIT_SUCCESS;
])],
    [AC_MSG_RESULT([yes])
     $2],
    [AC_MSG_RESULT([no])
     $3])
])
