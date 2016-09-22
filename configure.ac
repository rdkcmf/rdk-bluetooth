#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([btr-core], [1.0], [Mike.Kahn@arris.com Chandresh_Pitty@cable.comcast.com])
AC_CONFIG_MACRO_DIR([cfg])
AC_CONFIG_AUX_DIR([cfg])
AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign no-dist-gzip dist-bzip2 1.9])

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

LT_INIT
AC_LANG([C])

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

PKG_CHECK_MODULES([DBUS], [dbus-1])
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.32.0])

BTR_CORE_SRC="src"
BTR_CORE_TEST="tests"
BTR_IFCE_DBUS_BLUEZ4=" "
BTR_IFCE_DBUS_BLUEZ5=" "

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Check Btr-ifce
AC_ARG_ENABLE(btr-ifce,
              AS_HELP_STRING([--enable-btr-ifce], [Use bluez4/bluez5/custom]),
              [
                case "${enableval}" in
                    bluez4) have_bluez4="yes";;
                    bluez5) have_bluez5="yes";;
                    *) AC_MSG_ERROR([bad value ${enableval} for --enable-btr-ifce]) ;;
                esac
              ] ,
              [have_bluez4="yes"])

AS_IF([test "x$have_bluez4" = "xyes"], [
    AC_DEFINE(USE_BLUEZ4, 1, [Build with bluez4 support])
], [])

AS_IF([test "x$have_bluez5" = "xyes"], [
    AC_DEFINE(USE_BLUEZ5, 1, [Build with bluez5 support])
], [])

AM_CONDITIONAL([BTR_IFCE_DBUS_BLUEZ4], [test x$have_bluez4 = xyes])
AM_CONDITIONAL([BTR_IFCE_DBUS_BLUEZ5], [test x$have_bluez5 = xyes])


AC_SUBST(BTR_CORE_SRC)
AC_SUBST(BTR_CORE_TEST)

# Checks for library functions.

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 tests/Makefile])

AC_OUTPUT
