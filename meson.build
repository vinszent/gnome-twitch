project('gnome-twitch', 'c',
  version : '0.3.1',
  meson_version : '>= 0.36.0',
  license : 'GPL3',
  default_options: ['c_std=gnu11'])

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

meson.add_install_script('meson_post_install.sh')

# Common dependencies
libgtk_dep = dependency('gtk+-3.0', version : '>=3.20')
libpeas_dep = dependency('libpeas-1.0')

subdir('include')
subdir('src')
subdir('data')
subdir('po')

player_backend_install_dir = join_paths(get_option('libdir'),
  'gnome-twitch/player-backends')

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
