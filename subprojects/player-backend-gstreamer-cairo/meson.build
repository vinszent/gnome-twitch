project('player-backend-gstreamer-cairo', 'c',
        version : '0.2.0',
        meson_version : '>=0.31.0',
        license : 'GPL3',
        default_options : ['b_lundef=false'])

include_dir = include_directories('../../include/')

deps_gstreamer_cairo = [dependency('gtk+-3.0'),
                        dependency('libpeas-1.0'),
                        dependency('gstreamer-1.0')]

src_gstreamer_cairo = ['gt-player-backend-gstreamer-cairo.c']

lib_gstreamer_cairo = shared_library('player-backend-gstreamer-cairo', src_gstreamer_cairo,
                                     dependencies : deps_gstreamer_cairo,
                                     include_directories : include_dir,
                                     c_args : ['-std=gnu11', '-Wno-unused-variable'],
                                     install_dir : get_option('libdir') + '/gnome-twitch/plugins/player-backends/',
                                     install : true)

install_data(sources : 'player-backend-gstreamer-cairo.plugin',
             install_dir : get_option('libdir') + '/gnome-twitch/plugins/player-backends/')