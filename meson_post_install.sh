#!/bin/sh

glib-compile-schemas /usr/share/glib-2.0/schemas
update-desktop-database -q
gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor
