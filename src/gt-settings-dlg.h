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

#ifndef GT_SETTINGS_DLG_H
#define GT_SETTINGS_DLG_H

#include "gt-win.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_SETTINGS_DLG (gt_settings_dlg_get_type())

G_DECLARE_FINAL_TYPE(GtSettingsDlg, gt_settings_dlg, GT, SETTINGS_DLG, GtkDialog);

typedef enum _GtSettingsDlgView
{
    GT_SETTINGS_DLG_VIEW_GENERAL,
    GT_SETTINGS_DLG_VIEW_PLAYERS
} GtSettingsDlgView;

struct _GtSettingsDlg
{
    GtkDialog parent_instance;
};

GtSettingsDlg* gt_settings_dlg_new(GtWin* win);
void gt_settings_dlg_set_view(GtSettingsDlg* self, GtSettingsDlgView view);

G_END_DECLS

#endif
