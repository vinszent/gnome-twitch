Name:           gnome-twitch
Version:        0.1.0
Release:        2%{?dist}
Summary:        Enjoy Twitch on your GNU/Linux desktop

License:        GPLv3+
URL:            https://github.com/vinszent/gnome-twitch
Source0:        https://github.com/vinszent/gnome-twitch/archive/v%{version}-autotools.tar.gz

BuildRequires:  autoconf
BuildRequires:  autoconf-archive
BuildRequires:  automake
BuildRequires:  intltool
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
BuildRequires:  /usr/bin/desktop-file-validate

Requires:       gtk3
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
%setup -qn %{name}-%{version}-autotools

%build
autoreconf -i
%configure
make %{?_smp_mflags}

%install
%make_install
%find_lang %{name} --with-gnome

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/com.%{name}.app.desktop

%clean
rm -rf %{buildroot}

%post
/usr/bin/update-desktop-database &> /dev/null || :
/bin/touch --no-create %{_datadir}/icons/hicolor &> /dev/null || :

%postun
/usr/bin/update-desktop-database &> /dev/null || :
if [ $1 -eq 0 ] ; then
    /bin/touch --no-create %{_datadir}/icons/hicolor &> /dev/null || :
    /usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &> /dev/null || :
    /usr/bin/glib-compile-schemas %{_datadir}/glib-2.0/schemas &> /dev/null || :
fi

%posttrans
/usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &> /dev/null || :
/usr/bin/glib-compile-schemas %{_datadir}/glib-2.0/schemas &> /dev/null || :

%files -f %{name}.lang
%{_bindir}/%{name}
%{_datadir}/applications/com.%{name}.app.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
%{_datadir}/icons/hicolor/scalable/apps/%{name}.svg
%{_datadir}/glib-2.0/schemas/%{name}.gschema.xml

%changelog
* Wed Oct 21 2015 Vincent Szolnoky <vinszent@vinszent.com> - 0.1.0-2
- Corrected spec to confer better to packaging standards

* Tue Oct 20 2015 Vincent Szolnoky <vinszent@vinszent.com> - 0.1.0-1
- Initial package
