#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.64])
AC_INIT([libredis], [0.1.0])
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_SRCDIR([src/redis.c])
AC_CONFIG_HEADERS([config.h])

AM_MAINTAINER_MODE
AM_INIT_AUTOMAKE([-Wall -Werror])

GENERIC_LIBRARY_NAME=libredis

#release versioning
GENERIC_MAJOR_VERSION=0
GENERIC_MINOR_VERSION=1
GENERIC_MICRO_VERSION=0

#shared library versioning
GENERIC_LIBRARY_VERSION=0:0:0
AC_SUBST(GENERIC_LIBRARY_VERSION)

PACKAGE=$GENERIC_LIBRARY_NAME
AC_SUBST(GENERIC_LIBRARY_NAME)

GENERIC_VERSION=$GENERIC_MAJOR_VERSION.$GENERIC_MINOR_VERSION.$GENERIC_MICRO_VERSION
GENERIC_RELEASE=$GENERIC_MAJOR_VERSION.$GENERIC_MINOR_VERSION
AC_SUBST(GENERIC_RELEASE)
AC_SUBST(GENERIC_VERSION)

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX

AM_PROG_LIBTOOL
# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h netdb.h stdlib.h string.h sys/socket.h unistd.h printf.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset select socket strcasecmp strdup strtol strerror register_printf_type])

AC_CHECK_FUNCS(register_printf_specifier,
              AC_MSG_NOTICE(INFO: using register_printf_specifier))
if test "x$ac_cv_func_register_printf_specifier" = x""no; then
        AC_CHECK_FUNCS(register_printf_function,
                      AC_MSG_NOTICE(INFO: using register_printf_function))
        if test "x$ac_cv_func_register_printf_function" = x""no; then
                AC_MSG_ERROR(Neither register_printf_specifier nor register_printf_function found!)
        fi
fi
# check for gtk-doc
GTK_DOC_CHECK([1.11],[--copy --flavour no-tmpl])

AC_CONFIG_FILES([Makefile src/Makefile docs/Makefile])
AC_OUTPUT
