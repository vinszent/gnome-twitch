gnome = import('gnome')
gtk_dep = dependency('gtk+-3.0', version : '>=3.16')
soup_dep = dependency('libsoup-2.4')
json_dep = dependency('json-glib-1.0')
gst_dep = dependency('gstreamer-1.0')
gst_video_dep = dependency('gstreamer-video-1.0')

conf_data = configuration_data()
conf_data.set('version', VERSION)
conf_data.set('localedir', LOCALEDIR)
configure_file(input : 'config.h.in', output : 'config.h', install_dir : '.', configuration : conf_data)

res = gnome.compile_resources('gt-resources', '../data/gnome-twitch.gresource.xml', source_dir : '../data')
src = ['main.c', 'gt-app.c', 'gt-win.c', 'gt-twitch.c', 'gt-twitch-channel.c',
       'gt-player.c', 'gt-channels-view.c', 'gt-channels-view-child.c', 
       'gt-player-header-bar.c', 'gt-browse-header-bar.c', 'gt-twitch-game.c',
       'gt-games-view.c', 'gt-games-view-child.c', 'gt-settings-dlg.c', 
       'gt-enums.c', 'utils.c', res]


executable('gnome-twitch', src, dependencies : [gtk_dep, soup_dep, json_dep, gst_dep, gst_video_dep], install : true, c_args : '-Wno-unused-variable')
