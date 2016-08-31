msgfmt = find_program('msgfmt')

custom_target('desktop-file',
    input : 'com.vinszent.GnomeTwitch.desktop.in',
    output : 'com.vinszent.GnomeTwitch.desktop',
    command : [msgfmt, '--desktop', '--template', '@INPUT@', '-d', '../po', '-o', '@OUTPUT@'],
    #command : [intltool_merge, '-d', '-u', '../po', '@INPUT@', '@OUTPUT@'],
    install : true,
    install_dir : 'share/applications')

install_data('icons/hicolor/16x16/apps/gnome-twitch.png', install_dir : 'share/icons/hicolor/16x16/apps/')
install_data('icons/hicolor/22x22/apps/gnome-twitch.png', install_dir : 'share/icons/hicolor/22x22/apps')
install_data('icons/hicolor/24x24/apps/gnome-twitch.png', install_dir : 'share/icons/hicolor/24x24/apps')
install_data('icons/hicolor/32x32/apps/gnome-twitch.png', install_dir : 'share/icons/hicolor/32x32/apps')
install_data('icons/hicolor/48x48/apps/gnome-twitch.png', install_dir : 'share/icons/hicolor/48x48/apps')
install_data('icons/hicolor/256x256/apps/gnome-twitch.png', install_dir : 'share/icons/hicolor/256x256/apps')
install_data('icons/hicolor/512x512/apps/gnome-twitch.png', install_dir : 'share/icons/hicolor/512x512/apps')
install_data('icons/hicolor/scalable/apps/gnome-twitch.svg', install_dir : 'share/icons/hicolor/scalable/apps')
install_data('icons/hicolor/scalable/apps/gnome-twitch-symbolic.svg', install_dir : 'share/icons/hicolor/symbolic/apps')
install_data('com.vinszent.GnomeTwitch.gschema.xml', install_dir : 'share/glib-2.0/schemas/')
