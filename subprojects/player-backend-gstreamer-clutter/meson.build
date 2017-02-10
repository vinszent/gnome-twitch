project('player-backend-gstreamer-clutter', 'c',
        version : '0.2.0',
        meson_version : '>= 0.36.0',
        license : 'GPL3',
        default_options : ['b_lundef=false', 'c_std=gnu11'])

include_dir = include_directories('../../include/')
install_dir = join_paths(get_option('libdir'), 'gnome-twitch/plugins/player-backends')

deps_gstreamer_clutter = [dependency('gtk+-3.0'),
                          dependency('libpeas-1.0'),
                          dependency('clutter-gst-3.0'),
                          dependency('clutter-gtk-1.0')]

src_gstreamer_clutter = ['gt-player-backend-gstreamer-clutter.c']

lib_gstreamer_clutter = shared_library('player-backend-gstreamer-clutter', src_gstreamer_clutter,
                                     dependencies : deps_gstreamer_clutter,
                                     include_directories : include_dir,
                                     c_args : ['-Wno-unused-variable'],
                                     install_dir : install_dir,
                                     install : true)

install_data(sources : 'player-backend-gstreamer-clutter.plugin',
             install_dir : install_dir)
