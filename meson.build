project('gnome-twitch', 'c', version : '0.1.0')
add_global_arguments('-std=c11', language : 'c')

LOCALEDIR = get_option('prefix') + '/' + get_option('localedir')

meson.add_install_script('meson_post_install.sh')

subdir('src')
subdir('data')
subdir('po')
