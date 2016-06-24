project('player-backend-gstreamer-clutter', 'c',
        version : '0.2.0',
        meson_version : '>=0.31.0',
        license : 'GPL3',
        default_options : ['b_lundef=false'])

include_dir = include_directories('../../include/')

deps_gstreamer_clutter = [dependency('gtk+-3.0'),
                          dependency('libpeas-1.0'),
                          dependency('clutter-gst-3.0'),
                          dependency('clutter-gtk-1.0')]

src_gstreamer_clutter = ['gt-player-backend-gstreamer-clutter.c']

lib_gstreamer_clutter = shared_library('player-backend-gstreamer-clutter', src_gstreamer_clutter,
                                     dependencies : deps_gstreamer_clutter,
                                     include_directories : include_dir,
                                     c_args : ['-std=gnu11', '-Wno-unused-variable'],
                                     install_dir : get_option('libdir') + '/gnome-twitch/plugins/player-backends/',
                                     install : true)

install_data(sources : 'player-backend-gstreamer-clutter.plugin',
             install_dir : get_option('libdir') + '/gnome-twitch/plugins/player-backends/')