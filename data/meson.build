msgfmt = find_program('msgfmt')
podir = join_paths(meson.source_root(), 'po')
datadir = get_option('datadir')

custom_target('desktop-file',
    input : 'com.vinszent.GnomeTwitch.desktop.in',
    output : 'com.vinszent.GnomeTwitch.desktop',
    command : [msgfmt, '--desktop', '--template', '@INPUT@', '-d', podir, '-o', '@OUTPUT@'],
    install : true,
    install_dir : join_paths(datadir, 'applications'))

custom_target('appdata-file',
    input : 'com.vinszent.GnomeTwitch.appdata.xml.in',
    output : 'com.vinszent.GnomeTwitch.appdata.xml',
    command : [msgfmt, '--xml', '--template', '@INPUT@', '-d', podir, '-o', '@OUTPUT@'],
    install : true,
    install_dir : join_paths(datadir, 'appdata'))

icondir = join_paths(datadir, 'icons', 'hicolor')
icon_sizes = ['16x16', '24x24', '32x32', '48x48', '256x256', '512x512']

foreach size : icon_sizes
  install_data('icons/hicolor/@0@/apps/com.vinszent.GnomeTwitch.png'.format(size),
               install_dir : join_paths(icondir, size, 'apps'))
endforeach

install_data('icons/hicolor/scalable/apps/com.vinszent.GnomeTwitch.svg',
             install_dir : 'share/icons/hicolor/scalable/apps')

install_data('icons/hicolor/scalable/apps/com.vinszent.GnomeTwitch-symbolic.svg',
             install_dir : 'share/icons/hicolor/symbolic/apps')

install_data('com.vinszent.GnomeTwitch.gschema.xml',
             install_dir : join_paths(datadir, 'glib-2.0/schemas/'))
