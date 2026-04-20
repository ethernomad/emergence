Summary: Fast-paced multiplayer top-down arcade space-combat game
Name: emergence
Version: 0.9
Release: 1
License: GPL-3.0
Group: Amusements/Games
URL: https://github.com/bluedroplet/emergence
BuildRequires: meson >= 0.56, pkgconfig(sdl), pkgconfig(openssl), pkgconfig(zlib), pkgconfig(libpng), pkgconfig(x11), pkgconfig(xrandr), pkgconfig(alsa), pkgconfig(vorbisfile), pkgconfig(gl), pkgconfig(glu), vorbis-tools

%description
Emergence is a fast-paced multiplayer top-down 2D arcade space-combat game.
Players pilot craft in a space arena, pick up weapons, and battle for frags.

%build
meson setup builddir -Deditor=disabled
meson compile -C builddir

%install
meson install -C builddir --destdir=%{buildroot}

%files
%{_bindir}/em-client
%{_bindir}/em-server
%{_datadir}/emergence/
%{_datadir}/pixmaps/emergence.png
%{_datadir}/applications/em-client.desktop
%{_datadir}/applications/em-server.desktop
