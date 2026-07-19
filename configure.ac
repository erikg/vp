#############################################################################
# vp    -    SDL based image viewer for linux and fbsd. (X and console)	    #
# Copyright (C) 2001-2026 Erik Greenwald <erik@elfga.com>		    #
# 									    #
# This program is free software; you can redistribute it and/or modify      #
# it under the terms of the GNU General Public License as published by	    #
# the Free Software Foundation; either version 3 of the License, or         #
# (at your option) any later version.                                       #
#                                                                           #
# This program is distributed in the hope that it will be useful,           #
# but WITHOUT ANY WARRANTY; without even the implied warranty of            #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             #
# GNU General Public License for more details.                              #
#                                                                           #
# You should have received a copy of the GNU General Public License         #
# along with this program; if not, write to the Free Software               #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA #
#############################################################################

AC_INIT([vp], [1.9])
AC_CONFIG_SRCDIR([src/vp.c])
AC_CANONICAL_HOST
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([foreign])
AM_CONFIG_HEADER([config.h])

AC_PROG_CC
AC_PROG_INSTALL

AC_CHECK_HEADERS(unistd.h stdio.h stdlib.h string.h time.h)
AC_CHECK_FUNCS(mkstemps)
AC_CHECK_FUNC([ceil],[],[AC_CHECK_LIB([m],[ceil],[LIBS="$LIBS -lm"])])

AC_PATH_XTRA
dnl One combined check: SDL2_image.pc "Requires: sdl2", so querying the
dnl modules separately yields sdl2's libs twice; a single pkg-config call
dnl dedupes them.
PKG_CHECK_MODULES([SDL], [sdl2 >= 2.0.0 SDL2_image >= 2.0.0])

CFLAGS="$CFLAGS $SDL_CFLAGS $X_CFLAGS"
LIBS="$LIBS $SDL_LIBS $X_LIBS"

AC_SEARCH_LIBS(socket,socket wsock32)

dnl Optional https support via OpenSSL (>= 1.1 for TLS_client_method and
dnl SSL_set1_host). pkg-config first; fall back to a bare header+library
dnl probe for base-system OpenSSL installs that ship no openssl.pc
dnl (e.g. FreeBSD). Without it, https URLs report as unsupported at runtime.
AC_ARG_WITH([ssl],
    [AS_HELP_STRING([--with-ssl],
	[build https support via OpenSSL @<:@default=auto@:>@])],
    [], [with_ssl=auto])
have_ssl=no
AS_IF([test "x$with_ssl" != xno], [
    PKG_CHECK_MODULES([OPENSSL], [openssl >= 1.1],
	[have_ssl=yes
	 CFLAGS="$CFLAGS $OPENSSL_CFLAGS"
	 LIBS="$LIBS $OPENSSL_LIBS"],
	[AC_CHECK_HEADER([openssl/ssl.h],
	    [AC_CHECK_LIB([ssl], [TLS_client_method],
		[have_ssl=yes
		 LIBS="$LIBS -lssl -lcrypto"],
		[], [-lcrypto])])])
    AS_IF([test "x$have_ssl" = xyes],
	[AC_DEFINE([HAVE_OPENSSL], [1], [https support via OpenSSL])],
	[AS_IF([test "x$with_ssl" = xyes],
	    [AC_MSG_ERROR([--with-ssl given but OpenSSL >= 1.1 not found])],
	    [AC_MSG_NOTICE([OpenSSL not found - https support disabled])])])
])

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
