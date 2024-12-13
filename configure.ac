#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.70])

AC_CONFIG_AUX_DIR([build-aux])
AC_REQUIRE_AUX_FILE([git-version-gen])
AC_REQUIRE_AUX_FILE([gitlog-to-changelog])

AC_INIT([ws63flash],
	m4_esyscmd([build-aux/git-version-gen .tarball-version]),
	[gongzl@stu.hebust.edu.cn])

AC_CONFIG_SRCDIR([src/ws63flash.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])

AM_INIT_AUTOMAKE([-Wall])

# Checks for programs.
AC_PROG_CC

#AX_CHECK_COMPILE_FLAG([-Woverride-init], , , [-Werror])

# Includes for gnulib modules.
gl_EARLY
gl_INIT

# Checks for libraries.
AC_CHECK_LIB([m], [ceil])

# Checks for header files.
AC_CHECK_DECLS([B115200],[], AC_MSG_ERROR([B115200 not supported by header]), [[#include <termios.h>]])

# Non-POSIX baudrates
AC_CHECK_DECLS([B230400, B460800, B500000, B576000, B921600, B1000000, B1152000, B1500000, B2000000],[], [], [[#include <termios.h>]])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_CHECK_FUNCS([memset])

AC_CONFIG_FILES([Makefile
                 lib/Makefile
                 man/Makefile
                 src/Makefile])
AC_OUTPUT
