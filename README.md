# ![](/data/icons/hicolor/48x48/apps/gnome-twitch.png) GNOME Twitch
GNOME Twitch app for watching Twitch on your GNU/Linux desktop. Enjoy your favourite streams without
the hassle of flash or the web.

## Install
### Dependencies
* meson (install only)
* ninja (install only)
* gtk+-3.0 >= 3.16
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
mkdir build
meson . build
cd build
mesonconf -Dprefix=/usr
ninja install
```
### Distro packages
* [Arch linux]("https://aur4.archlinux.org/packages/gnome-twitch-git/")

## Screenshots
![](/data/screenshots/scrot_streams.png?raw=true)
![](/data/screenshots/scrot_games.png?raw=true)
![](/data/screenshots/scrot_player.png?raw=true)
