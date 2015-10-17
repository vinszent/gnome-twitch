bin_PROGRAMS = gnome-twitch

gnome_twitch_CFLAGS = -std=c11 $(APP_CFLAGS)
gnome_twitch_LDADD = $(APP_LIBS) -lm
gnome_twitch_CPPFLAGS = -DLOCALEDIR=\""$(localedir)"\"


BUILT_SOURCES = gt-resources.c gt-resources.h

gt-resources.c gt-resources.h: $(top_srcdir)/data/gnome-twitch.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(top_srcdir)/data --generate-dependencies $(top_srcdir)/data/gnome-twitch.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(top_srcdir)/data/gnome-twitch.gresource.xml --sourcedir=$(top_srcdir)/data --target=$@ --generate --c-name gt

gnome_twitch_SOURCES = \
		       main.c \
		       gt-app.c \
		       gt-win.c \
		       gt-twitch.c \
		       gt-channel.c \
		       gt-player.c \
		       gt-player-clutter.c \
		       gt-channels-view.c \
		       gt-channels-container-child.c \
		       gt-channels-container.c \
		       gt-channels-container-top.c \
		       gt-channels-container-search.c \
		       gt-channels-container-game.c \
		       gt-channels-container-favourite.c \
		       gt-games-container.c \
		       gt-games-container-child.c \
		       gt-games-container-top.c \
		       gt-games-container-search.c \
		       gt-favourites-view.c \
		       gt-player-header-bar.c \
		       gt-browse-header-bar.c \
		       gt-game.c \
		       gt-games-view.c \
		       gt-settings-dlg.c \
		       gt-favourites-manager.c \
		       gt-enums.c \
		       utils.c 

nodist_gnome_twitch_SOURCES = \
			      gt-resources.c

CLEANFILES = \
	     gt-resources.c \
	     gt-resources.h

