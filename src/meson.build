# This file is part of GNOME Twitch - 'Enjoy Twitch on your GNU/Linux desktop'
# Copyright © 2017 Vincent Szolnoky <vinszent@vinszent.com>
#
# GNOME Twitch is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# GNOME Twitch is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNOME Twitch. If not, see <http://www.gnu.org/licenses/>.

gnome = import('gnome')
pkg = import('pkgconfig')

ver = vcs_tag(input : 'version.h.in', output : 'version.h',
  command : ['../print_git_version.py'],
  fallback : meson.project_version())

conf_data = configuration_data()
conf_data.set_quoted('GT_LOCALE_DIR', LOCALE_DIR)
conf_data.set_quoted('GT_LIB_DIR', LIB_DIR)
conf_data.set('USE_DEPRECATED_WEBKIT', get_option('use-deprecated-webkit'))
conf_data.set_quoted('CLIENT_ID', 'afjnp6n4ufzott4atb3xpb8l5a31aav')
configure_file(output : 'config.h', configuration : conf_data)

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

res = gnome.compile_resources('gt-resources',
  '../data/com.vinszent.GnomeTwitch.gresource.xml',
  source_dir : '../data')

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
  'gt-vod.c',
  'gt-channel-vod-container.c',
  'gt-vod-container-child.c',
  'gt-settings-dlg.c',
  'gt-follows-manager.c',
  'gt-twitch-login-dlg.c',
  'gt-twitch-channel-info-dlg.c',
  'gt-irc.c',
  'gt-chat.c',
  'gt-enums.c',
  'gt-resource-downloader.c',
  'utils.c',
  res,
  ver
]

src_gt_library = [
  'gt-player-backend.c'
]

gt_executable_c_args = default_c_args

gt_executable_link_args = []

gt_library_c_args = gt_executable_c_args

api = '0.2'

if host_machine.system() == 'windows'
  gt_executable_link_args += '-mwindows'
endif

  # We need to do this because Windows doesn't allow DLLs to be linked
  # with undefined references. Therefore we move all shared functionality into a
  # sub-library that plugins can be linked against.
gt_library = shared_library('gnome-twitch-' + api, src_gt_library,
  include_directories : include_dir,
  dependencies : deps_gt_library,
  install : true,
  c_args : gt_library_c_args)

gt_library_dep = declare_dependency(
  include_directories : include_dir,
  link_with : gt_library)

pkg.generate(
  libraries : gt_library,
  name : 'gnome-twitch-' + api,
  description : 'Library used for building GNOME Twitch plugins',
  requires : ['gtk+-3.0', 'libpeas-1.0'],
  install : true,
  version : meson.project_version())

gnome.generate_gir(gt_library,
  sources: [join_paths('..', 'include', 'gnome-twitch', 'gt-player-backend.h')],
  nsversion: api,
  namespace: 'Gt',
  dependencies: deps_gt_library,
  includes: ['Gtk-3.0'],
  install: true,
  install_dir_gir: join_paths(get_option('datadir'), 'gir-1.0'),
  install_dir_typelib: join_paths(get_option('libdir'), 'girepository-1.0')
)

deps_gt += gt_library_dep

executable('gnome-twitch', src_gt_executable,
  include_directories : include_dir,
  dependencies : deps_gt,
  install : true,
  link_args : gt_executable_link_args,
  c_args : gt_executable_c_args)
