bin_PROGRAMS = iview
iview_SOURCES = \
	image.c\
	image.h\
	input.c\
	input.h\
	iview.c\
	iview.h\
	ll.c\
	ll.h\
	timer.c\
	timer.h

lint:
	for a in *.[ch] ; do echo $$a ; lclint $$a @CFLAGS@ @SDL_CFLAGS@  > $$a.lint ; done
indent:
	indent *.[ch]
gindent:
	gindent *.[ch]
