#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([btr-core], [1.0], [Thomson_silvera@cable.comcast.com])
AC_CONFIG_MACRO_DIR([cfg])
AC_CONFIG_AUX_DIR([cfg])
AM_INIT_AUTOMAKE([subdir-objects  -Wall -Werror foreign no-dist-gzip dist-bzip2 1.9])

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

LT_INIT
AC_LANG([C++])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])],
[AM_DEFAULT_VERBOSITY=1
AC_SUBST(AM_DEFAULT_VERBOSITY)])


# Checks for programs.
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h unistd.h])
#PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.24.0])
#PKG_CHECK_MODULES([GTHREAD], [gthread-2.0 >= 2.24.0])

PKG_CHECK_MODULES([DBUS], [dbus-1])

BTR_CORE_SRC="src"
BTR_CORE_TEST="tests"

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T

AC_SUBST(BTR_CORE_SRC)
AC_SUBST(BTR_CORE_TEST)

# Checks for library functions.

#AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([Makefile
                 src/Makefile
                 tests/Makefile])

AC_OUTPUT
