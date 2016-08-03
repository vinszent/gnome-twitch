# ![](/data/icons/hicolor/48x48/apps/gnome-twitch.png) GNOME Twitch
GNOME Twitch app for watching Twitch on your GNU/Linux desktop. Enjoy your favourite streams without
the hassle of flash or the web.

[![Issues in ready](https://badge.waffle.io/vinszent/gnome-twitch.svg?label=ready&title=Ready)](http://waffle.io/vinszent/gnome-twitch) [![Issues in progress](https://badge.waffle.io/vinszent/gnome-twitch.svg?label=in%20progress&title=In%20progress)](http://waffle.io/vinszent/gnome-twitch)

[![Gitter](https://badges.gitter.im/vinszent/gnome-twitch.svg)](https://gitter.im/vinszent/gnome-twitch?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge) (free tech support and development help ![datsheffy](https://static-cdn.jtvnw.net/emoticons/v1/170/1.0))

## News
### Click [here](NEWS.md) for the entire news log
* __24/06/2016__
    * Kind of old news but if you are logged in, GT will now get your follows from Twitch. Just be sure to refresh your login token so that GT can get new permissions if you haven't already.
    * GT now has a plugin system. Currently it's only for player backends but later on I will expand the API for other types of plugins.
    * There are now 4 backends available: gstreamer-clutter, gstreamer-cairo, gstreamer-opengl, mpv-opengl so feel free to try them out!

## Install
### Dependencies
* meson >= 0.32.0 (install only)
* ninja (install only)
* gtk+-3.0 >= 3.20
* libsoup
* json-glib
* webkit2gtk

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

_Note: If you undo commit [6382b8b](https://github.com/vinszent/gnome-twitch/commit/6382b8b918306306da0c014cedb8f314ecd66a93) then meson => 0.26.0 can be used_

### From source

``` shell
mkdir build
cd build
meson --prefix /usr --libdir lib -Ddo-post-install=true -Dwith-player-gstreamer-cairo=true ..
ninja install
```

### Install extra player backends
#### Root install

``` shell
cd subprojects/${gt-player-backend-you-want}
mkdir build
cd build
meson --prefix /usr --libdir lib ..
sudo ninja install
```
#### Local install

Same as the root install but instead change the last two lines to:

``` shell
meson --prefix ~/.local --libdir share ..
ninja install
```

### Distro packages
* [Arch linux](https://aur.archlinux.org/packages/gnome-twitch/)
* [Arch linux (git)](https://aur.archlinux.org/packages/gnome-twitch-git/)
* [Debian (courtesy of @dengelt)](https://tracker.debian.org/pkg/gnome-twitch/)
* [Fedora](https://copr.fedoraproject.org/coprs/ippytraxx/gnome-twitch/) (You will need to install gstreamer1-libav from RPMFusion)
* [Ubuntu (courtesy of GetDeb.net)](http://www.getdeb.net/app/GNOME%20Twitch) (You will need to install the ubuntu-restricted-extras for the h264 decoder)
* [Ubuntu (courtesy of @Sunderland93)](https://launchpad.net/~samoilov-lex/+archive/ubuntu/gnome-twitch) (Same requirements as above)
* [Ubuntu (courtesy of WebUpd8.org)](https://launchpad.net/~nilarimogard/+archive/ubuntu/webupd8/+index?batch=75&memo=150&start=150) (Same requirements as above)

## Screenshots
![](/data/screenshots/scrot_player.png?raw=true)
![](/data/screenshots/scrot_streams.png?raw=true)
