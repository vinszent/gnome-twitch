/*
 *  This file is part of GNOME Twitch - 'Enjoy Twitch on your GNU/Linux desktop'
 *  Copyright © 2017 Hyunjun Ko <zzoon@igalia.com>
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

#ifndef GT_PLAYER_BACKEND_GSTREAMER_VAAPI_H_
#define GT_PLAYER_BACKEND_GSTREAMER_VAAPI_H_

#include <gtk/gtk.h>
#include <libpeas/peas.h>

G_BEGIN_DECLS

#define GT_TYPE_PLAYER_BACKEND_GSTREAMER_VAAPI gt_player_backend_gstreamer_vaapi_get_type()

G_DECLARE_FINAL_TYPE(GtPlayerBackendGstreamerVaapi, gt_player_backend_gstreamer_vaapi, GT, PLAYER_BACKEND_GSTREAMER_VAAPI, PeasExtensionBase);

struct _GtPlayerBackendGstreamerVaapi
{
    PeasExtensionBase parent_instance;
};

G_MODULE_EXPORT void peas_register_types(PeasObjectModule* module);

G_END_DECLS

#endif
