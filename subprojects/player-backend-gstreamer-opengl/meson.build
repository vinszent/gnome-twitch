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

gstreamer_dep = dependency('gstreamer-1.0')

deps_gstreamer_opengl = [
  libgtk_dep,
  libpeas_dep,
  gstreamer_dep,
  gt_module_dep
]

src_gstreamer_opengl = [
  'gt-player-backend-gstreamer-opengl.c'
]

shared_module('player-backend-gstreamer-opengl', src_gstreamer_opengl,
  dependencies : deps_gstreamer_opengl,
  install_dir : player_backend_install_dir,
  install : true)

install_data(sources : 'player-backend-gstreamer-opengl.plugin',
  install_dir : player_backend_install_dir)
