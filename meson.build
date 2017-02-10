project('gnome-twitch', 'c',
        version : '0.3.1',
        meson_version : '>= 0.36.0',
        license : 'GPL3',
        default_options: [
          'c_std=gnu11',
          'b_lundef=false',
        ])

# Used in config.h
LOCALE_DIR = join_paths(get_option('prefix'), get_option('localedir'))
LIB_DIR = join_paths(get_option('prefix'), get_option('libdir'))
DATA_DIR = join_paths(get_option('prefix'), get_option('datadir'))

WITH_PLAYER_GSTREAMER_CLUTTER = get_option('with-player-gstreamer-clutter')
WITH_PLAYER_MPV_OPENGL = get_option('with-player-mpv-opengl')
WITH_PLAYER_GSTREAMER_CAIRO = get_option('with-player-gstreamer-cairo')
WITH_PLAYER_GSTREAMER_OPENGL = get_option('with-player-gstreamer-opengl')

meson.add_install_script('meson_post_install.sh')

if WITH_PLAYER_GSTREAMER_CLUTTER
   subproject('player-backend-gstreamer-clutter')
endif
if WITH_PLAYER_MPV_OPENGL
   subproject('player-backend-mpv-opengl')
endif
if WITH_PLAYER_GSTREAMER_CAIRO
   subproject('player-backend-gstreamer-cairo')
endif
if WITH_PLAYER_GSTREAMER_OPENGL
   subproject('player-backend-gstreamer-opengl')
endif

subdir('include')
subdir('src')
subdir('data')
subdir('po')
