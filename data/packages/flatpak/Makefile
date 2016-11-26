.PHONY: all install

all:
	ninja -C build/gnome-twitch/
	ninja -C build/player-backend-gstreamer-cairo/
	ninja -C build/player-backend-gstreamer-clutter/
	ninja -C build/player-backend-gstreamer-opengl/

install:
	ninja -C build/gnome-twitch/                     install
	ninja -C build/player-backend-gstreamer-cairo/   install
	ninja -C build/player-backend-gstreamer-clutter/ install
	ninja -C build/player-backend-gstreamer-opengl/  install
