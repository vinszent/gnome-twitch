gnome = import('gnome')
gtk_dep = dependency('gtk+-3.0', version : '>=3.16')
soup_dep = dependency('libsoup-2.4')
json_dep = dependency('json-glib-1.0')
gst_dep = dependency('gstreamer-1.0')
gst_video_dep = dependency('gstreamer-video-1.0')

conf_data = configuration_data()
ver = vcs_tag(input : 'version.h.in', output : 'version.h', 
              command : ['sh', '../print_git_version.sh'], 
              fallback : meson.project_version())
conf_data.set('LOCALE_DIR', LOCALEDIR)
configure_file(input : 'config.h.in', output : 'config.h', install_dir : '.', configuration : conf_data)

res = gnome.compile_resources('gt-resources', '../data/gnome-twitch.gresource.xml', source_dir : '../data')
src = ['main.c', 'gt-app.c', 'gt-win.c', 'gt-twitch.c', 'gt-channel.c',
       'gt-player.c', 'gt-channels-view.c', 'gt-channels-container-child.c', 
       'gt-channels-container.c', 'gt-channels-container-top.c',
       'gt-channels-container-favourite.c', 'gt-channels-container-search.c',
       'gt-channels-container-game.c', 'gt-games-container.c', 'gt-games-container-top.c',
       'gt-games-container-search.c', 'gt-favourites-view.c',
       'gt-player-header-bar.c', 'gt-browse-header-bar.c', 'gt-game.c',
       'gt-games-view.c', 'gt-games-container-child.c', 'gt-settings-dlg.c', 
       'gt-favourites-manager.c', 'gt-enums.c', 'utils.c', res, ver]


executable('gnome-twitch', src, dependencies : [gtk_dep, soup_dep, json_dep, gst_dep, gst_video_dep], install : true, c_args : '-Wno-unused-variable')
