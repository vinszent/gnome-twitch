gnome = import('gnome')

ver = vcs_tag(input : 'version.h.in', output : 'version.h',
  command : ['sh', '../print_git_version.sh'],
  fallback : meson.project_version())

conf_data = configuration_data()
conf_data.set_quoted('GT_LOCALE_DIR', LOCALE_DIR)
conf_data.set_quoted('GT_LIB_DIR', LIB_DIR)
conf_data.set('USE_DEPRECATED_WEBKIT', get_option('use-deprecated-webkit'))
configure_file(input : 'config.h.in', output : 'config.h', configuration : conf_data)

libsoup_dep = dependency('libsoup-2.4')
json_glib_dep = dependency('json-glib-1.0')
libpeas_gtk_dep = dependency('libpeas-gtk-1.0')

deps_gt = [
  libgtk_dep,
  libpeas_dep,
  libpeas_gtk_dep,
  libsoup_dep,
  json_glib_dep
]

if get_option('use-deprecated-webkit')
   deps_gt += dependency('webkitgtk-3.0')
else
   deps_gt += dependency('webkit2gtk-4.0')
endif

if libgtk_dep.get_pkgconfig_variable('targets').contains('x11')
   deps_gt += dependency('x11')
 endif

deps_gt_library = [
  libgtk_dep,
  libpeas_dep
]

cc = meson.get_compiler('c')
deps_gt += cc.find_library('m', required: false)

res = gnome.compile_resources('gt-resources', '../data/com.vinszent.GnomeTwitch.gresource.xml', source_dir : '../data')

src_gt_executable = [
  'main.c',
  'gt-app.c',
  'gt-win.c',
  'gt-twitch.c',
  'gt-channel.c',
  'gt-player.c',
  'gt-item-container.c',
  'gt-top-channel-container.c',
  'gt-search-channel-container.c',
  'gt-followed-channel-container.c',
  'gt-game-channel-container.c',
  'gt-top-game-container.c',
  'gt-search-game-container.c',
  'gt-container-view.c',
  'gt-channel-container-view.c',
  'gt-followed-container-view.c',
  'gt-game-container-view.c',
  'gt-channels-container-child.c',
  'gt-player-header-bar.c',
  'gt-browse-header-bar.c',
  'gt-game.c',
  'gt-games-container-child.c',
  'gt-settings-dlg.c',
  'gt-follows-manager.c',
  'gt-twitch-login-dlg.c',
  'gt-twitch-channel-info-dlg.c',
  'gt-irc.c',
  'gt-chat.c',
  'gt-enums.c',
  'utils.c',
  res,
  ver
]

src_gt_library = [
  'gt-player-backend.c'
]

if BUILD_GNOME_TWITCH_LIBRARY or BUILD_GNOME_TWITCH_EXECUTABLE
  gt_library = shared_library('gnome-twitch', src_gt_library,
    include_directories : include_dir,
    dependencies : deps_gt_library,
    install : true,
    c_args : ['-Wno-unused-variable'])
endif


executable('gnome-twitch', src_gt_executable,
  include_directories : include_dir,
  dependencies : deps_gt,
  install : true,
  link_with : gt_library,
  c_args : '-Wno-unused-variable')
