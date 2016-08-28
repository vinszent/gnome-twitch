project('player-backend-gstreamer-opengl', 'c',
        version : '0.2.0',
        meson_version : '>=0.31.0',
        license : 'GPL3',
        default_options : ['b_lundef=false'])

include_dir = include_directories('../../include/')

deps_gstreamer_opengl = [dependency('gtk+-3.0'),
                        dependency('libpeas-1.0'),
                        dependency('gstreamer-1.0')]

src_gstreamer_opengl = ['gt-player-backend-gstreamer-opengl.c']

lib_gstreamer_opengl = shared_library('player-backend-gstreamer-opengl', src_gstreamer_opengl,
                                     dependencies : deps_gstreamer_opengl,
                                     include_directories : include_dir,
                                     c_args : ['-std=gnu11', '-Wno-unused-variable'],
                                     install_dir : get_option('libdir') + '/gnome-twitch/plugins/player-backends/',
                                     install : true)

install_data(sources : 'player-backend-gstreamer-opengl.plugin',
             install_dir : get_option('libdir') + '/gnome-twitch/plugins/player-backends/')