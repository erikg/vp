dnl $Id: configure.ac,v 1.2 2004/03/27 00:33:34 erik Exp $

AC_INIT
AC_CONFIG_SRCDIR([src/vp.c])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE(vp, 1.6)
AM_CONFIG_HEADER([config.h])

AC_PROG_CC
AC_PROG_INSTALL

AC_CHECK_HEADERS(unistd.h stdio.h stdlib.h string.h time.h)
AC_CHECK_FUNC(mkstemps,AC_DEFINE(HAVE_MKSTEMPS,,Safe mktemp))

AM_PATH_SDL(1.2.0,,AC_MSG_ERROR(["SDL 1.2.0 or better is required. Get it at http://libsdl.org."]))

CFLAGS="$CFLAGS $SDL_CFLAGS"
LIBS="$LIBS $SDL_LIBS -lSDL_image"

AC_CHECK_LIB(SDL_image,IMG_Load,,AC_MSG_ERROR([SDL_image is required http://www.libsdl.org/projects/SDL_image/index.html]))

GETOPT_LONG_DIR=
AC_CHECK_FUNCS(getopt_long , [GETOPT_LONG_DIR=""], [
    AC_CHECK_LIB(gnugetopt, getopt_long, LIBS="$LIBS -lgnugetopt", [
        GETOPT_LONG_DIR="getopt_long"
        CFLAGS="$CFLAGS -I../getopt_long"
        LIBS="$LIBS ../getopt_long/libgetopt_long.a"
        AC_DEFINE(NO_GETOPT_LONG,,[Missing getopt_long])])])
AC_SUBST(GETOPT_LONG_DIR)

AC_CONFIG_FILES([Makefile doc/Makefile src/Makefile vp.spec])
AC_OUTPUT
