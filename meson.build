project('gnome-twitch', 'c',
        version : '0.3.0',
        meson_version : '>=0.31.0',
        license : 'GPL3')

add_global_arguments('-std=gnu11', language : 'c')

LOCALE_DIR = get_option('prefix') + '/' + get_option('localedir')
LIB_DIR = get_option('prefix') + '/' + get_option('libdir')
DATA_DIR = get_option('prefix') + '/' + get_option('datadir')
WITH_PLAYER_GSTREAMER_CLUTTER = get_option('with-player-gstreamer-clutter')
WITH_PLAYER_MPV_OPENGL = get_option('with-player-mpv-opengl')
WITH_PLAYER_GSTREAMER_CAIRO = get_option('with-player-gstreamer-cairo')
WITH_PLAYER_GSTREAMER_OPENGL = get_option('with-player-gstreamer-opengl')

if get_option('do-post-install')
    meson.add_install_script('meson_post_install.sh')
endif

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
