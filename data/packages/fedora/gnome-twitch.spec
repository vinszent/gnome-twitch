%global appid com.vinszent.GnomeTwitch

Name:           gnome-twitch
Version:        0.3.1
Release:        1%{?dist}
Summary:        Enjoy Twitch on your GNU/Linux desktop

License:        GPLv3+
URL:            https://github.com/vinszent/gnome-twitch
Source0:        https://github.com/vinszent/gnome-twitch/archive/v%{version}.tar.gz

BuildRequires:  meson >= 0.36.0
BuildRequires:  gettext
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gtk+-3.0) >= 3.2.0
BuildRequires:  pkgconfig(gstreamer-1.0)
BuildRequires:  pkgconfig(libsoup-2.4)
BuildRequires:  pkgconfig(json-glib-1.0)
BuildRequires:  pkgconfig(libpeas-gtk-1.0)
BuildRequires:  pkgconfig(webkit2gtk-4.0)
BuildRequires:  libappstream-glib


Requires:        gstreamer1-plugins-good
# Note that this requires rpmfusion...
Requires:        gstreamer1-libav
# opengl backend
Requires:        gstreamer1(element-gtkglsink)
# cairo backend
Requires:        gstreamer1(element-gtksink)

%description
Enjoy Twitch on your GNU/Linux desktop

%prep
%autosetup -p1

%build
# NOTE: These options change in the next release
%meson -Dwith-player-gstreamer-cairo=true -Dwith-player-gstreamer-opengl=true -Ddo-post-install=false -Db_lundef=false
%meson_build

%install
%meson_install
rm -rf %{buildroot}/%{_includedir}
%find_lang gnome-twitch

# NOTE: Added next release
#%check
#/usr/bin/appstream-util validate-relax --nonet %{buildroot}/%{_datadir}/appdata/*.appdata.xml

%post
/bin/touch --no-create %{_datadir}/icons/hicolor &> /dev/null || :

%postun
if [ $1 -eq 0 ] ; then
    /bin/touch --no-create %{_datadir}/icons/hicolor &> /dev/null || :
    /usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &> /dev/null || :
fi

%posttrans
/usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &> /dev/null || :

%files -f gnome-twitch.lang
%license LICENSE
%dir %{_libdir}/%{name}
%dir %{_libdir}/%{name}/plugins
%dir %{_libdir}/%{name}/plugins/player-backends
%{_bindir}/%{name}
%{_libdir}/%{name}/plugins/player-backends/*
%{_datadir}/applications/%{appid}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
%{_datadir}/icons/hicolor/scalable/apps/%{name}.svg
%{_datadir}/icons/hicolor/symbolic/apps/%{name}-symbolic.svg
%{_datadir}/glib-2.0/schemas/%{appid}.gschema.xml
#%{_datadir}/appdata/*.appdata.xml

%changelog
* Wed Mar 8 2017 Patrick Griffis <tingping@fedoraproject.org>
- Cleanup and update to 0.3.1

* Sun Aug 7 2016 Vincent Szolnoky <vinszent@vinszent.com>
- Update to v0.2.1

* Fri Apr 8 2016 Vincent Szolnoky <vinszent@vinszent.com>
- Initial package
