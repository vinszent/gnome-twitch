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

test_cflags = [
  '-Wno-unused-variable',
  '-Wno-unused-parameter',
  '-Wno-missing-field-initializers'
]

default_c_args = []
cc = meson.get_compiler('c')
foreach cflag : test_cflags
  if cc.has_argument(cflag)
    default_c_args += cflag
  endif
endforeach

# Used in config.h
LOCALE_DIR = join_paths(get_option('prefix'), get_option('localedir'))
LIB_DIR = join_paths(get_option('prefix'), get_option('libdir'))
DATA_DIR = join_paths(get_option('prefix'), get_option('datadir'))

player_backend_options = get_option('build-player-backends').split(',')

BUILD_EXECUTABLE = get_option('build-executable')
BUILD_PLAYER_BACKEND_GSTREAMER_CAIRO = player_backend_options.contains('gstreamer-cairo')
BUILD_PLAYER_BACKEND_GSTREAMER_OPENGL = player_backend_options.contains('gstreamer-opengl')
BUILD_PLAYER_BACKEND_GSTREAMER_CLUTTER = player_backend_options.contains('gstreamer-clutter')
BUILD_PLAYER_BACKEND_MPV_OPENGL = player_backend_options.contains('mpv-opengl')

include_dir = include_directories('include')

# Common dependencies
libgtk_dep = dependency('gtk+-3.0', version : '>=3.20')
libpeas_dep = dependency('libpeas-1.0')

# This will possibly override later
gt_library_dep = declare_dependency(
  include_directories : include_dir)

if BUILD_EXECUTABLE
  subdir('data')
  subdir('po')
  subdir('include')
  subdir('src')

  meson.add_install_script('meson_post_install.py')
elif host_machine.system() == 'windows'
  # We aren't building the main executable and we are on Windows
  # but we still could possibly be building plugins.
  # Therefore we need to declare the sub-library as a dep that
  # should already exist on the system (otherwise why are you
  # trying to build plugins for an app that you don't have installed).
  gt_library_dep = dependency('gnome-twitch')
endif

player_backend_install_dir = join_paths(get_option('libdir'),
  'gnome-twitch', 'player-backends')

if BUILD_PLAYER_BACKEND_GSTREAMER_CLUTTER
  subdir('player-backends/player-backend-gstreamer-clutter')
endif
if BUILD_PLAYER_BACKEND_MPV_OPENGL
  subdir('player-backends/player-backend-mpv-opengl')
endif
if BUILD_PLAYER_BACKEND_GSTREAMER_CAIRO
  subdir('player-backends/player-backend-gstreamer-cairo')
endif
if BUILD_PLAYER_BACKEND_GSTREAMER_OPENGL
  subdir('player-backends/player-backend-gstreamer-opengl')
endif
