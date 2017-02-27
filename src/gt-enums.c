/*
 *  This file is part of GNOME Twitch - 'Enjoy Twitch on your GNU/Linux desktop'
 *  Copyright © 2017 Vincent Szolnoky <vinszent@vinszent.com>
 *
 *  GNOME Twitch is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GNOME Twitch is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNOME Twitch. If not, see <http://www.gnu.org/licenses/>.
 */

#include "gt-enums.h"
#include "gt-twitch.h"
#include "gt-settings-dlg.h"

static const GEnumValue gt_settings_dlg_view_enum_values[] =
{
    {GT_SETTINGS_DLG_VIEW_GENERAL, "GT_SETTINGS_DLG_GENERAL", "general"},
    {GT_SETTINGS_DLG_VIEW_PLAYERS, "GT_SETTINGS_DLG_PLUGINS", "plugins"}
};

GType
gt_settings_dlg_view_get_type()
{
    static GType type = 0;

    if (!type)
        type = g_enum_register_static("GtSettingsDlgView", gt_settings_dlg_view_enum_values);

    return type;
}
