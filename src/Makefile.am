# $Id: Makefile.am,v 1.7 2002/12/11 04:22:22 erik Exp $

bin_PROGRAMS = vp

vp_SOURCES = ftp.c http.c image.c input.c vp.c net.c timer.c
noinst_HEADERS = ftp.h http.h image.h input.h vp.h net.h timer.h

lint:
	for a in *.[ch] ; do echo $$a ; lclint $$a @CFLAGS@ @SDL_CFLAGS@  > $$a.lint ; done
indent:
	indent *.[ch]
gindent:
	gindent *.[ch]
