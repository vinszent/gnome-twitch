# ![](/data/icons/hicolor/48x48/apps/gnome-twitch.png) GNOME Twitch
GNOME Twitch app for watching Twitch on your GNU/Linux desktop. Enjoy your favourite streams without
the hassle of flash or the web.

[![Gitter](https://badges.gitter.im/Ippytraxx/gnome-twitch.svg)](https://gitter.im/Ippytraxx/gnome-twitch?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)

## Install
### Dependencies
* meson >= 0.31.0 (install only)
* ninja (install only)
* gtk+-3.0 >= 3.20
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

_Note: If you undo commit c4b49557dfed8465f273f2b5490002607baa5182 then gtk+-3.0 >= 3.16 can be used_
_Note: If you undo commit 6382b8b918306306da0c014cedb8f314ecd66a93 then meson => 0.26.0 can be used_

### From source
```
mkdir build
cd build
meson --prefix /usr -Ddo-post-install=true ..
ninja install
```

### Distro packages
* [Arch linux](https://aur.archlinux.org/packages/gnome-twitch/)
* [Arch linux (git)](https://aur.archlinux.org/packages/gnome-twitch-git/)
* [Debian (courtesy of @dengelt)] (https://tracker.debian.org/pkg/gnome-twitch/)
* [Fedora](https://copr.fedoraproject.org/coprs/ippytraxx/gnome-twitch/) (You will need to install gstreamer1-libav from RPMFusion)
* [Ubuntu (courtesy of GetDeb.net)](http://www.getdeb.net/app/GNOME%20Twitch) (You will need to install the ubuntu-restricted-extras for the h264 decoder)
* [Ubuntu (courtesy of @Sunderland93)](https://launchpad.net/~samoilov-lex/+archive/ubuntu/gnome-twitch) (Same requirements as above)
* [Ubuntu (courtesy of WebUpd8.org)](https://launchpad.net/~nilarimogard/+archive/ubuntu/webupd8/+index?batch=75&memo=150&start=150) (Same requirements as above)

## Screenshots
![](/data/screenshots/scrot_player.png?raw=true)
![](/data/screenshots/scrot_streams.png?raw=true)