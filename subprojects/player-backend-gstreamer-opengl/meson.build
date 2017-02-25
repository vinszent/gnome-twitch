gstreamer_dep = dependency('gstreamer-1.0')

deps_gstreamer_opengl = [
  libgtk_dep,
  libpeas_dep,
  gstreamer_dep
]

src_gstreamer_opengl = [
  'gt-player-backend-gstreamer-opengl.c'
]

lib_gstreamer_opengl = shared_library('player-backend-gstreamer-opengl', src_gstreamer_opengl,
  dependencies : deps_gstreamer_opengl,
  link_with : gt_library,
  include_directories : include_dir,
  c_args : ['-Wno-unused-variable'],
  install_dir : player_backend_install_dir,
  install : true)

install_data(sources : 'player-backend-gstreamer-opengl.plugin',
  install_dir : player_backend_install_dir)
