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

datadir = get_option('datadir')

if (host_machine.system() != 'windows')
  i18n = import('i18n')

  i18n.merge_file(
    input : 'com.vinszent.GnomeTwitch.desktop.in',
    output : 'com.vinszent.GnomeTwitch.desktop',
    po_dir : '../po',
    type : 'desktop',
    install : true,
    install_dir : join_paths(datadir, 'applications'))

  i18n.merge_file(
    input : 'com.vinszent.GnomeTwitch.appdata.xml.in',
    output : 'com.vinszent.GnomeTwitch.appdata.xml',
    po_dir : '../po',
    install : true,
    install_dir : join_paths(datadir, 'appdata'))
endif

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
