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

project('gnome-twitch', 'c',
  version : '0.3.1',
  meson_version : '>= 0.37.0',
  license : 'GPL3',
  default_options: [
    'c_std=gnu11',
    'warning_level=2',
  ]
)

# Used in config.h
LOCALE_DIR = join_paths(get_option('prefix'), get_option('localedir'))
LIB_DIR = join_paths(get_option('prefix'), get_option('libdir'))
DATA_DIR = join_paths(get_option('prefix'), get_option('datadir'))

BUILD_GNOME_TWITCH_EXECUTABLE = get_option('build-gnome-twitch-executable')
BUILD_GNOME_TWITCH_LIBRARY = get_option('build-gnome-twitch-library')
BUILD_PLAYER_BACKEND_GSTREAMER_CLUTTER = get_option('build-player-backend-gstreamer-clutter')
BUILD_PLAYER_BACKEND_MPV_OPENGL = get_option('build-player-backend-mpv-opengl')
BUILD_PLAYER_BACKEND_GSTREAMER_CAIRO = get_option('build-player-backend-gstreamer-cairo')
BUILD_PLAYER_BACKEND_GSTREAMER_OPENGL = get_option('build-player-backend-gstreamer-opengl')

include_dir = include_directories('include')

# Common dependencies
libgtk_dep = dependency('gtk+-3.0', version : '>=3.20')
libpeas_dep = dependency('libpeas-1.0')

if BUILD_GNOME_TWITCH_EXECUTABLE
  subdir('data')
  subdir('po')

  meson.add_install_script('meson_post_install.sh')
endif

if BUILD_GNOME_TWITCH_LIBRARY
  subdir('include')
endif

subdir('src')

player_backend_install_dir = join_paths(get_option('libdir'),
  'gnome-twitch', 'player-backends')

if BUILD_PLAYER_BACKEND_GSTREAMER_CLUTTER
  subdir('subprojects/player-backend-gstreamer-clutter')
endif
if BUILD_PLAYER_BACKEND_MPV_OPENGL
  subdir('subprojects/player-backend-mpv-opengl')
endif
if BUILD_PLAYER_BACKEND_GSTREAMER_CAIRO
  subdir('subprojects/player-backend-gstreamer-cairo')
endif
if BUILD_PLAYER_BACKEND_GSTREAMER_OPENGL
  subdir('subprojects/player-backend-gstreamer-opengl')
endif
