bin_PROGRAMS = gnome-twitch

gnome_twitch_CFLAGS = -std=c11 $(APP_CFLAGS)
gnome_twitch_LDADD = $(APP_LIBS)
gnome_twitch_LDFLAGS = -std=c11
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
		       gt-channels-view.c \
		       gt-channels-view-child.c \
		       gt-player-header-bar.c \
		       gt-browse-header-bar.c \
		       gt-game.c \
		       gt-games-view.c \
		       gt-games-view-child.c \
		       gt-settings-dlg.c \
		       gt-favourites-manager.c \
		       gt-enums.c \
		       utils.c 

nodist_gnome_twitch_SOURCES = \
			      gt-resources.c

CLEANFILES = \
	     gt-resources.c \
	     gt-resources.h

