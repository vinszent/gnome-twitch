gstreamer_dep = dependency('gstreamer-1.0')

deps_gstreamer_cairo = [
  libgtk_dep,
  libpeas_dep,
  gstreamer_dep
]

src_gstreamer_cairo = [
  'gt-player-backend-gstreamer-cairo.c'
]

lib_gstreamer_cairo = shared_library('player-backend-gstreamer-cairo', src_gstreamer_cairo,
  dependencies : deps_gstreamer_cairo,
  link_with : gt_library,
  include_directories : include_dir,
  c_args : ['-Wno-unused-variable'],
  install_dir : player_backend_install_dir,
  install : true)

install_data(sources : 'player-backend-gstreamer-cairo.plugin',
  install_dir : player_backend_install_dir)
