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

# Linux icons
for i in 16 22 24 32 48 256 512
do
    inkscape -z -e hicolor/$i\x$i/apps/com.vinszent.GnomeTwitch.png -w $i -h $i com.vinszent.GnomeTwitch.svg
done

# Windows icon
icotool -c \
        hicolor/16x16/apps/com.vinszent.GnomeTwitch.png \
        hicolor/32x32/apps/com.vinszent.GnomeTwitch.png \
        hicolor/48x48/apps/com.vinszent.GnomeTwitch.png \
        -o com.vinszent.GnomeTwitch.ico
