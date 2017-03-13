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

mpv_dep = dependency('mpv')
epoxy_dep = dependency('epoxy')

deps_mpv_opengl = [
  libgtk_dep,
  mpv_dep,
  libpeas_dep,
  mpv_dep,
  epoxy_dep,
  gt_library_dep
]

src_mpv_opengl = [
  'gt-player-backend-mpv-opengl.c'
]

shared_module('player-backend-mpv-opengl', src_mpv_opengl,
  dependencies : deps_mpv_opengl,
  install_dir : player_backend_install_dir,
  install : true)

install_data(sources : 'player-backend-mpv-opengl.plugin',
  install_dir : player_backend_install_dir)
