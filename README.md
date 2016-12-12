# ![](/data/icons/hicolor/48x48/apps/com.vinszent.GnomeTwitch.png) GNOME Twitch

Enjoy Twitch on your GNU/Linux desktop.

[![Gitter](https://badges.gitter.im/vinszent/gnome-twitch.svg)](https://gitter.im/gnome-twitch/Lobby?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge) (free tech support and development help ![datsheffy](https://static-cdn.jtvnw.net/emoticons/v1/170/1.0))

## [Latest news](http://gnome-twitch.vinszent.com/posts/gnome-twitch-v0.3.0-post.html)

## Install
### Dependencies
* meson >= 0.36.0 (install only)
* ninja (install only)
* gtk+-3.0 >= 3.20
* libsoup
* json-glib
* webkit2gtk or webkitgtk with `-Duse-deprecated-webkit=true` flag

#### Player backend - GStreamer Cairo & GStreamer OpenGL
* gstreamer-1.0
* gst-libav
* gst-plugins-base
* gst-plugins-good
* gst-plugins-bad

#### Player backend - GStreamer Clutter
Same as above plus:

* clutter-gst-3.0
* clutter-gtk-1.0

#### Player backend - MPV OpenGL
* mpv

_Note: If you undo commit [c4b4955](https://github.com/vinszent/gnome-twitch/commit/c4b49557dfed8465f273f2b5490002607baa5182) then gtk+-3.0 >= 3.16 can be used_

### From source

``` shell
meson build -Dwith-player-gstreamer-cairo=true
ninja -C build
sudo ninja -C build install
```

### Install extra player backends
#### Root install

``` shell
cd subprojects/${gt-player-backend-you-want}
meson build
ninja -C build
sudo ninja -C build install
```
#### Local install

Same as the root install but instead change the last two lines to:

``` shell
meson build --prefix=$HOME/.local
ninja -C build install
```

### Packages
* [Arch linux](https://aur.archlinux.org/packages/gnome-twitch/)
* [Arch linux (MPV player backend plugin)](https://aur.archlinux.org/packages/gnome-twitch-mpv/)
* [Arch linux (git)](https://aur.archlinux.org/packages/gnome-twitch-git/)
* [Debian (courtesy of @dengelt)](https://tracker.debian.org/pkg/gnome-twitch/)
* [Fedora](https://copr.fedoraproject.org/coprs/ippytraxx/gnome-twitch/) (You will need to install gstreamer1-libav from RPMFusion)
* [Flatpak](https://github.com/vinszent/gnome-twitch/wiki/How-to-install-FlatPak-package)
* [Ubuntu (courtesy of GetDeb.net)](http://www.getdeb.net/app/GNOME%20Twitch) (You will need to install the ubuntu-restricted-extras for the h264 decoder)
* [Ubuntu (courtesy of @Sunderland93)](https://launchpad.net/~samoilov-lex/+archive/ubuntu/gnome-twitch) (Same requirements as above)
* [Ubuntu (courtesy of WebUpd8.org)](https://launchpad.net/~nilarimogard/+archive/ubuntu/webupd8/+index?batch=75&memo=150&start=150) (Same requirements as above)
* [Gentoo](https://github.com/TorArneThune/gnome-twitch-ebuild)

## Screenshots
![](/data/screenshots/scrot_player.png?raw=true)
![](/data/screenshots/scrot_streams.png?raw=true)
