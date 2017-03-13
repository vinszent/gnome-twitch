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

clutter_gst_dep = dependency('clutter-gst-3.0')
clutter_gtk_dep = dependency('clutter-gtk-1.0')

deps_gstreamer_clutter = [
  gtk_dep,
  libpeas_dep,
  clutter_gst_dep,
  clutter_gtk_dep,
  gt_library_dep
]

src_gstreamer_clutter = [
  'gt-player-backend-gstreamer-clutter.c'
]

shared_module('player-backend-gstreamer-clutter', src_gstreamer_clutter,
  c_args : default_c_args,
  dependencies : deps_gstreamer_clutter,
  install_dir : player_backend_install_dir,
  install : true)

install_data(sources : 'player-backend-gstreamer-clutter.plugin',
  install_dir : player_backend_install_dir)
