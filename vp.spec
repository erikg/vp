Summary: view image files.
Name: vp
Version: 1.5
Release: 1
Copyright: GPL
Vendor: Erik Greenwald
Url: http://math.smsu.edu/~erik/software.php?id=63
Group: Amusements/Graphics
Source0: http://math.smsu.edu/~erik/files/vp-1.5.tar.gz
Buildroot: /usr/src/redhat/BUILD/view

%description
Image viewer based on SDL that can read and display a wide variety of formats.
Features include both fullscreen and windowed mode, rescaling images to fill
the screen, ability to navigate the images, slideshow, full functionality
both in X and in console, and the ability to load images from web servers.

%prep
%setup -q
%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=/usr

make

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install

%clean
rm -rf $RPM_BUILD_ROOT

%files
/usr/*

%changelog

