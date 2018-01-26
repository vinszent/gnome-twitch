# ![](/data/icons/hicolor/48x48/apps/com.vinszent.GnomeTwitch.png) GNOME Twitch

Enjoy Twitch on your GNU/Linux desktop.

<!-- ## [Latest news](http://gnome-twitch.vinszent.com/posts/gnome-twitch-v0.3.0-post.html) -->

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

### From source

``` shell
meson build
sudo ninja -C build install
```

### Install player backends
#### Root install
``` shell
meson build \
    -Dbuild-executable=false \
    -Dbuild-player-backends=${PLAYER_BACKENDS_YOU_WANT}
sudo ninja -C build install
```

**Note:** `${PLAYER_BACKENDS_YOU_WANT}` should be replaced with a
comma separated list of
`gstreamer-opengl,gstreamer-cairo,gstreamer-clutter,mpv-opengl`, for
example `-Dbuild-player-backends=gstreamer-cairo,mpv-opengl`

#### Local install
``` shell
meson build --prefix=$HOME/.local --libdir=share \
    -Dbuild-executable=false \
    -Dbuild-player-backends=${PLAYER_BACKENDS_YOU_WANT}
ninja -C build install
```

### Packages
* [Flatpak (courtesy of @TingPing)](https://github.com/vinszent/gnome-twitch/wiki/How-to-install-FlatPak-package)
* [Arch linux](https://aur.archlinux.org/packages/gnome-twitch/)
* [Arch linux (git)](https://aur.archlinux.org/packages/gnome-twitch-git/)
* [Debian (courtesy of @dengelt)](https://tracker.debian.org/pkg/gnome-twitch/)
* [Ubuntu (courtesy of WebUpd8.org)](https://launchpad.net/%7Enilarimogard/+archive/ubuntu/webupd8/+index?batch=75&direction=backwards&memo=150&start=75) (Same requirements as above)
* [Gentoo (courtesy of @TorArneThune)](https://github.com/TorArneThune/gnome-twitch-ebuild)
* [Solus](https://packages.solus-project.com/shannon/g/gnome-twitch/)
* [OpenBSD](http://openports.se/multimedia/gnome-twitch)

To install extra backends, please refer to either instructions above or checkout the [wiki page](https://github.com/vinszent/gnome-twitch/wiki/How-to-install-player-backends)
for details on which packages to install for the common distros.

## Screenshots
![](/data/screenshots/scrot_player.png?raw=true)
![](/data/screenshots/scrot_streams.png?raw=true)
