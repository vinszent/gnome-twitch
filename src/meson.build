gnome = import('gnome')

ver = vcs_tag(input : 'version.h.in', output : 'version.h',
              command : ['sh', '../print_git_version.sh'],
              fallback : meson.project_version())

conf_data = configuration_data()
conf_data.set_quoted('GT_LOCALE_DIR', LOCALE_DIR)
conf_data.set_quoted('GT_LIB_DIR', LIB_DIR)
conf_data.set('USE_DEPRECATED_WEBKIT', get_option('use-deprecated-webkit'))
configure_file(input : 'config.h.in', output : 'config.h', configuration : conf_data)

libgtk_dep = dependency('gtk+-3.0', version : '>=3.20')

deps_gt = [libgtk_dep,
           dependency('libsoup-2.4'),
           dependency('json-glib-1.0'),
           dependency('libpeas-1.0'),
           dependency('libpeas-gtk-1.0')]

if get_option('use-deprecated-webkit')
   deps_gt += dependency('webkitgtk-3.0')
else
   deps_gt += dependency('webkit2gtk-4.0')
endif

if libgtk_dep.get_pkgconfig_variable('targets').contains('x11')
   deps_gt += dependency('x11')
endif

cc = meson.get_compiler('c')
deps_gt += cc.find_library('m', required: false)

res = gnome.compile_resources('gt-resources', '../data/com.vinszent.GnomeTwitch.gresource.xml', source_dir : '../data')

src_gt = ['main.c',
         'gt-app.c',
         'gt-win.c',
         'gt-twitch.c',
         'gt-channel.c',
         'gt-player.c',
         'gt-player-backend.c',
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
         ver]

executable('gnome-twitch', src_gt,
           include_directories : include_dir,
           dependencies : deps_gt,
           install : true,
           c_args : '-Wno-unused-variable')
