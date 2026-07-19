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
# along with this program.  If not, see <https://www.gnu.org/licenses/>.    #
#############################################################################

SUBDIRS = doc src

# Ship the CMake build system, the debian/ packaging, and the test suite in
# the dist tarball too, so a release tarball is not autotools-only (CMake is
# the preferred build) and `dpkg-buildpackage -b` works straight out of an
# unpacked tarball.
EXTRA_DIST = README.md ChangeLog $(PACKAGE).spec TODO \
	CMakeLists.txt config.h.cmake.in \
	cmake/FindSDL2.cmake cmake/FindSDL2_image.cmake \
	src/CMakeLists.txt doc/CMakeLists.txt \
	tests/net-tests.sh tests/testserv.py \
	debian/changelog debian/compat debian/control debian/copyright \
	debian/rules debian/source/format

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
