# vp — view picture

[![ci](https://github.com/erikg/vp/actions/workflows/ci.yml/badge.svg)](https://github.com/erikg/vp/actions/workflows/ci.yml)

An SDL2-based image viewer and slideshow for Linux, FreeBSD, and macOS.
First released in 2001 (as `iview`), and still doing one thing: showing
your images fast, with no fuss.

## Features

- Views anything SDL2_image can decode (png, jpeg, webp, avif, tiff, ...)
- Slideshow mode with configurable delay
- Pan and zoom (keyboard or mouse), fit-to-screen or 1:1, fullscreen
- On-screen filename display
- Views images straight off the web: `http://` and `https://` URLs are
  downloaded and shown; redirects followed, IPv6 supported, certificates
  verified against the system trust store (`-k` to override, `-K` to keep
  the downloaded files)
- GPU-accelerated scaling via the SDL2 renderer, with software fallback

## Usage

```
vp [-fhkKlvz] [-s <seconds>] [-r <width>x<height>] file-or-url ...
```

| Key | Action |
|-----|--------|
| q / esc / x | quit |
| f | toggle fullscreen |
| z | fit-to-screen / 1:1 |
| left / right | previous / next image |
| shift+arrows | pan |
| + / - | zoom |
| enter | redisplay current image, stop slideshow |
| space | toggle slideshow |
| n / N | filename display / cycle its position |

See the man page for the rest.

## Building

Needs SDL2 and SDL2_image; OpenSSL is optional (for https).

CMake (preferred):

```
cmake -B build
cmake --build build
build/src/vp <images>
```

Autotools also works (`autoreconf -fi && ./configure && make`), and is
the basis of the release tarball and the deb/rpm packaging
(`dpkg-buildpackage -b`, `rpmbuild -ta vp-<version>.tar.gz`).

Dependencies:

- Debian/Ubuntu: `apt install libsdl2-dev libsdl2-image-dev libssl-dev`
- Fedora/RHEL: `dnf install SDL2-devel SDL2_image-devel openssl-devel`
  (RHEL-family needs EPEL + CRB enabled for SDL2 at runtime)
- FreeBSD: `pkg install cmake pkgconf sdl2 sdl2_image`
- macOS: `brew install sdl2 sdl2_image pkg-config openssl@3`

The network code has a self-contained test suite:
`sh tests/net-tests.sh <path-to-vp>` (needs python3 and the openssl CLI).

## License

GPLv3 or later. See [COPYING](COPYING).
