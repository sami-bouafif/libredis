#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.64])
AC_INIT([libredis], [0.1-alpha0])
AC_CONFIG_SRCDIR([redis.c])
AC_CONFIG_HEADERS([config.h])

AM_MAINTAINER_MODE
AM_INIT_AUTOMAKE([-Wall -Werror])
AM_PROG_LIBTOOL

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h netdb.h stdlib.h string.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset select socket strcasecmp strdup strtol])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
