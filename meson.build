project('gnome-twitch', 'c', version : '0.2.0')
add_global_arguments('-std=gnu11', language : 'c')

LOCALEDIR = get_option('prefix') + '/' + get_option('localedir')

if get_option('do-post-install')
    meson.add_install_script('meson_post_install.sh')
endif

subdir('src')
subdir('data')
subdir('po')
