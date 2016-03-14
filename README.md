# ![](/data/icons/hicolor/48x48/apps/gnome-twitch.png) GNOME Twitch
GNOME Twitch app for watching Twitch on your GNU/Linux desktop. Enjoy your favourite streams without
the hassle of flash or the web.

## Install
### Dependencies
* meson >= 0.26.0 (install only)
* ninja (install only)
* gtk+-3.0 >= 3.16
* libsoup
* json-glib
* gstreamer-1.0
* gst-libav
* gst-plugins-base
* gst-plugins-good
* gst-plugins-bad
* clutter-gst
* clutter-gtk
* webkit2gtk

### From source
```
mkdir build
cd build
meson --prefix /usr -Ddo-post-install=true ..
ninja install
```

### Distro packages
* [Arch linux](https://aur4.archlinux.org/packages/gnome-twitch/)
* [Arch linux (git)](https://aur4.archlinux.org/packages/gnome-twitch-git/)
* [Debian] (https://tracker.debian.org/pkg/gnome-twitch/)
* [Fedora](https://copr.fedoraproject.org/coprs/ippytraxx/gnome-twitch/) (You will need to install gstreamer1-libav from RPMFusion)
* [Ubuntu (PPA)](https://launchpad.net/~ippytraxx/+archive/ubuntu/gnome-twitch/) (You will need to install the ubuntu-restricted-extras for the h264 decoder)
* [Ubuntu (courtesy of GetDeb.net)](http://www.getdeb.net/app/GNOME%20Twitch) (Same requirements as the PPA)

## Screenshots
![](/data/screenshots/scrot_player.png?raw=true)
![](/data/screenshots/scrot_streams.png?raw=true)
![](/data/screenshots/scrot_games.png?raw=true)
