# $Id: Makefile.am,v 1.4 2004/01/02 13:04:50 erik Exp $

SUBDIRS = @GETOPT_LONG_DIR@ doc src

EXTRA_DIST=ChangeLog $(PACKAGE).spec TODO

.PHONY : rpm
rpm: dist
	cp $(PACKAGE)-$(VERSION).tar.gz ${HOME}/rpm/SOURCES/
	rpmbuild -ba $(PACKAGE).spec
	cp ${HOME}/rpm/RPMS/i386/$(PACKAGE)-$(VERSION)-*.i386.rpm \
	   ${HOME}/rpm/SRPMS/$(PACKAGE)-$(VERSION)-*.src.rpm .


.PHONY : deb
deb: 
	debian/rules
	debuild
	debc
	debi
	release

packages: rpm

lint:
	make -C src lint
indent:
	make -C src indent
