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

i18n = import('i18n')

langs = ['de', 'es', 'pl', 'pt', 'sv', 'fr', 'ru', 'sr', 'hu', 'nl', 'cs']

i18n.gettext('gnome-twitch', languages : langs, args : [
  '--from-code=UTF-8',
  '--add-comments',

  '--keyword=_',
  '--keyword=N_',
  '--keyword=C_:1c,2',
  '--keyword=NC_:1c,2',
  '--keyword=g_dcgettext:2',
  '--keyword=g_dngettext:2,3',
  '--keyword=g_dpgettext2:2c,3',

  '--flag=N_:1:pass-c-format',
  '--flag=C_:2:pass-c-format',
  '--flag=NC_:2:pass-c-format',
  '--flag=g_dngettext:2:pass-c-format',
  '--flag=g_strdup_printf:1:c-format',
  '--flag=g_string_printf:2:c-format',
  '--flag=g_string_append_printf:2:c-format',
  '--flag=g_error_new:3:c-format',
  '--flag=g_set_error:4:c-format',
])
