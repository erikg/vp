# $Id: Makefile.am,v 1.8 2002/12/11 04:39:50 erik Exp $

bin_PROGRAMS = vp

vp_SOURCES = ftp.c http.c image.c input.c vp.c net.c timer.c
noinst_HEADERS = ftp.h http.h image.h input.h vp.h net.h timer.h

lint:
	for a in *.[ch] ; do echo $$a ; splint $$a @CFLAGS@ @SDL_CFLAGS@  > $$a.lint ; done
indent:
	indent *.[ch]
gindent:
	gindent *.[ch]
