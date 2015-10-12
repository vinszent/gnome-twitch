# ![](/data/icons/hicolor/48x48/apps/gnome-twitch.png) GNOME Twitch
GNOME Twitch app for watching Twitch on your GNU/Linux desktop. Enjoy your favourite streams without
the hassle of flash or the web.

**This branch is ONLY for backwards compatability and distros that ABSOLUTELY can not get Meson to work. The latest features will always come to the Meson branch first and then this one when I have time.**

## Install
### Dependencies
* gtk+-3.0 >= 3.16
* intltool
* libsoup
* json-glib
* gstreamer-1.0
* gst-libav
* gst-plugins-base
* gst-plugins-good
* gst-plugins-bad
* gst-plugins-ugly

### From source
```
autoreconf -i
./configure --prefix=/usr
make
make install
```
#### Post install
```
glib-compile-schemas /usr/share/glib-2.0/schemas
update-desktop-database -q
gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor
```
### Distro packages
* [Arch linux](https://aur4.archlinux.org/packages/gnome-twitch-git/)
* [Ubuntu](http://www.getdeb.net/app/GNOME%20Twitch)

## Screenshots
![](/data/screenshots/scrot_streams.png?raw=true)
![](/data/screenshots/scrot_games.png?raw=true)
![](/data/screenshots/scrot_player.png?raw=true)
