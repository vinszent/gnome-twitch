gnome = import('gnome')

conf_data = configuration_data()
ver = vcs_tag(input : 'version.h.in', output : 'version.h',
              command : ['sh', '../print_git_version.sh'],
              fallback : meson.project_version())
conf_data.set('LOCALE_DIR', LOCALEDIR)
configure_file(input : 'config.h.in', output : 'config.h', configuration : conf_data)
res = gnome.compile_resources('gt-resources', '../data/gnome-twitch.gresource.xml', source_dir : '../data')

deps = [dependency('gtk+-3.0', version : '>=3.16'),
        dependency('libsoup-2.4'),
        dependency('json-glib-1.0'),
        dependency('clutter-gst-3.0'),
        dependency('clutter-gtk-1.0'),
        dependency('webkit2gtk-4.0')]
#        dependency('mpv'),
#        dependency('epoxy')]

src = ['main.c',
       'gt-app.c',
       'gt-win.c',
       'gt-twitch.c',
       'gt-channel.c',
       'gt-player.c',
       'gt-channels-view.c',
       'gt-channels-container-child.c',
       'gt-channels-container.c',
       'gt-channels-container-top.c',
       'gt-channels-container-favourite.c',
       'gt-channels-container-search.c',
       'gt-channels-container-game.c',
       'gt-games-container.c',
       'gt-games-container-top.c',
       'gt-games-container-search.c',
       'gt-favourites-view.c',
       'gt-player-clutter.c',
 #      'gt-player-mpv.c',
       'gt-player-header-bar.c',
       'gt-browse-header-bar.c',
       'gt-game.c',
       'gt-games-view.c',
       'gt-games-container-child.c',
       'gt-settings-dlg.c',
       'gt-favourites-manager.c',
       'gt-twitch-login-dlg.c',
       'gt-twitch-channel-info-dlg.c',
       'gt-twitch-chat-client.c',
       'gt-twitch-chat-view.c',
       'gt-enums.c',
       'utils.c',
       res,
       ver]


executable('gnome-twitch', src, dependencies : deps, install : true, c_args : '-Wno-unused-variable', link_args : '-lm')
