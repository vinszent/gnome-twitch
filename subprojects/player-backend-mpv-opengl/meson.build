project('player-backend-mpv-opengl', 'c',
        version : '0.2.0',
        meson_version : '>= 0.36.0',
        license : 'GPL3',
        default_options : ['b_lundef=false', 'c_std=gnu11'])

include_dir = include_directories('../../include/')
install_dir = join_paths(get_option('libdir'), 'gnome-twitch/plugins/player-backends')

deps_mpv_opengl = [dependency('gtk+-3.0'),
                   dependency('libpeas-1.0'),
                   dependency('mpv'),
                   dependency('epoxy')]

src_mpv_opengl = ['gt-player-backend-mpv-opengl.c']

lib_mpv_opengl = shared_library('player-backend-mpv-opengl', src_mpv_opengl,
                                dependencies : deps_mpv_opengl,
                                include_directories : include_dir,
                                c_args : ['-Wno-unused-variable'],
                                install_dir : install_dir,
                                install : true)

install_data(sources : 'player-backend-mpv-opengl.plugin',
             install_dir : install_dir)
