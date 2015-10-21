Name:           gnome-twitch
Version:        0.1.0
Release:        1%{?dist}
Summary:        Enjoy Twitch on your GNU/Linux desktop

License:        GPLv3+
URL:            https://github.com/Ippytraxx/gnome-twitch
Source0:        https://github.com/Ippytraxx/gnome-twitch/archive/v%{version}.tar.gz

BuildRequires:  meson >= 0.26.0
BuildRequires: 	ninja-build
BuildRequires:	gettext
BuildRequires:	pkgconfig(clutter-1.0)
BuildRequires: 	pkgconfig(clutter-gst-3.0)
BuildRequires: 	pkgconfig(clutter-gtk-1.0)
BuildRequires: 	pkgconfig(gstreamer-1.0)
BuildRequires: 	pkgconfig(gstreamer-base-1.0)
BuildRequires: 	pkgconfig(gstreamer-plugins-base-1.0)
BuildRequires: 	pkgconfig(gstreamer-plugins-bad-1.0)
BuildRequires: 	pkgconfig(gstreamer-audio-1.0)
BuildRequires: 	pkgconfig(gstreamer-video-1.0)
BuildRequires: 	pkgconfig(gtk+-3.0) >= 3.16
BuildRequires: 	pkgconfig(libsoup-2.4)
BuildRequires: 	pkgconfig(json-glib-1.0)

Requires:       gstreamer1
Requires:       gstreamer1-plugins-base
Requires:       gstreamer1-plugins-good
Requires:       gstreamer1-plugins-bad-free

Suggests:       gstreamer1-vaapi
Suggests:       libva
Suggests:       libva-vdpau-driver

%description
Enjoy Twitch on your GNU/Linux desktop

%prep
%setup -q

%build
mkdir build
meson . build
cd build
mesonconf -Dbuildtype=release

ninja-build

%install
cd build
mesonconf -Dprefix=%{_prefix}
DESTDIR=%{buildroot} ninja-build install

%clean
rm -rf %{buildroot}

%post
%desktop_database_post
%icon_theme_cache_post
glib-compile-schemas %{_datadir}/glib-2.0/schemas &> /dev/null

%postun
%desktop_database_postun
%icon_theme_cache_postun
glib-compile-schemas %{_datadir}/glib-2.0/schemas &> /dev/null

%files
%{_bindir}/%{name}
%{_datadir}/applications/com.%{name}.app.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
%{_datadir}/icons/hicolor/scalable/apps/%{name}.svg
%{_datadir}/glib-2.0/schemas/%{name}.gschema.xml
%{_datadir}/locale/*/LC_MESSAGES/%{name}.mo

%changelog
* Tue Oct 20 2015 Vincent Szolnoky <ippytraxx@installgentoo.com>
- Initial package
