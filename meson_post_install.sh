#!/bin/sh

# This file is part of GNOME Twitch - 'Enjoy Twitch on your GNU/Linux desktop'
# Copyright © 2017 Vincent Szolnoky <vinszent@vinszent.com>
#
# GNOME Twitch is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# GNOME Twitch is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNOME Twitch. If not, see <http://www.gnu.org/licenses/>.

# Packagers set DESTDIR so we don't want to try writing to root
if [ -z $DESTDIR ]; then

	PREFIX=${MESON_INSTALL_PREFIX:-/usr}

	echo 'Compiling GSchema'
	glib-compile-schemas "$PREFIX/share/glib-2.0/schemas"
	echo 'Updating desktop database'
	update-desktop-database -q
	echo 'Updating icon cache'
	gtk-update-icon-cache -q -t -f "$PREFIX/share/icons/hicolor"

fi
