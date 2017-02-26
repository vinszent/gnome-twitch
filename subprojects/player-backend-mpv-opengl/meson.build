mpv_dep = dependency('mpv')
epoxy_dep = dependency('epoxy')

deps_mpv_opengl = [
  mpv_dep,
  libpeas_dep,
  mpv_dep,
  epoxy_dep,
  gt_library_dep
]

src_mpv_opengl = [
  'gt-player-backend-mpv-opengl.c'
]

lib_mpv_opengl = shared_library('player-backend-mpv-opengl', src_mpv_opengl,
  dependencies : deps_mpv_opengl,
  include_directories : include_dir,
  c_args : ['-Wno-unused-variable'],
  install_dir : player_backend_install_dir,
  install : true)

install_data(sources : 'player-backend-mpv-opengl.plugin',
  install_dir : player_backend_install_dir)
