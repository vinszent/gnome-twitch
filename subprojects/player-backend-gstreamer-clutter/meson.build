clutter_gst_dep = dependency('clutter-gst-3.0')
clutter_gtk_dep = dependency('clutter-gtk-1.0')

deps_gstreamer_clutter = [
  gtk_dep,
  libpeas_dep,
  clutter_gst_dep,
  clutter_gtk_dep,
  gt_library_dep
]

src_gstreamer_clutter = [
  'gt-player-backend-gstreamer-clutter.c'
]

lib_gstreamer_clutter = shared_library('player-backend-gstreamer-clutter', src_gstreamer_clutter,
  dependencies : deps_gstreamer_clutter,
  include_directories : include_dir,
  c_args : ['-Wno-unused-variable'],
  install_dir : player_backend_install_dir,
  install : true)

install_data(sources : 'player-backend-gstreamer-clutter.plugin',
  install_dir : player_backend_install_dir)
