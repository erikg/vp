Summary: view image files.
Name: iview
Version: 1.4
Release: 1
Copyright: GPL
Vendor: Erik Greenwald
Url: http://math.smsu.edu/~erik/software.php?id=63
Group: Amusements/Graphics
Source0: http://math.smsu.edu/~erik/files/iview-1.4.tar.gz
Buildroot: /usr/src/redhat/BUILD/view

%description
Image viewer based on SDL that can read and display a wide variety of formats.
features include both fullscreen and windowed mode, rescaling images to fill
the screen, ability to navigate the images, slideshow, and full functionality
both in X and in console.

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

