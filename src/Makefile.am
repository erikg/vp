bin_PROGRAMS = vp

vp_SOURCES = ftp.c http.c image.c input.c vp.c net.c timer.c
noinst_HEADERS = ftp.h http.h image.h input.h vp.h net.h timer.h

lint:
	for a in *.[ch] ; do echo $$a ; lclint $$a @CFLAGS@ @SDL_CFLAGS@  > $$a.lint ; done
indent:
	indent *.[ch]
gindent:
	gindent *.[ch]
