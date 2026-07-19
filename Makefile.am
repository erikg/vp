#############################################################################
# vp    -    SDL based image viewer for linux and fbsd. (X and console)	    #
# Copyright (C) 2001-2026 Erik Greenwald <erik@elfga.com>		    #
# 									    #
# This program is free software; you can redistribute it and/or modify      #
# it under the terms of the GNU General Public License as published by	    #
# the Free Software Foundation; either version 2 of the License, or         #
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

SUBDIRS = @GETOPT_LONG_DIR@ doc src

# Ship the CMake build system and the test suite in the dist tarball too,
# so a release tarball is not autotools-only (CMake is the preferred build).
EXTRA_DIST = ChangeLog $(PACKAGE).spec TODO \
	CMakeLists.txt config.h.cmake.in \
	src/CMakeLists.txt doc/CMakeLists.txt \
	tests/net-tests.sh tests/testserv.py

.PHONY : rpm
rpm: dist
	cp $(PACKAGE)-$(VERSION).tar.gz ${HOME}/rpmbuild/SOURCES/
	rpmbuild -ba $(PACKAGE).spec
	cp ${HOME}/rpmbuild/RPMS/i386/$(PACKAGE)-$(VERSION)-*.i386.rpm \
	   ${HOME}/rpmbuild/SRPMS/$(PACKAGE)-$(VERSION)-*.src.rpm .


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
