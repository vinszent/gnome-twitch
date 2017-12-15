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

#include "gnome-twitch/gt-player-backend.h"

G_DEFINE_INTERFACE(GtPlayerBackend, gt_player_backend, G_TYPE_OBJECT)

static void
gt_player_backend_default_init(GtPlayerBackendInterface* iface)
{
    g_object_interface_install_property(iface,
        g_param_spec_double("volume", "Volume", "Current volume",
            0, 1.0, 0.3, G_PARAM_READWRITE));

    g_object_interface_install_property(iface,
        g_param_spec_boolean("playing", "Playing", "Whether playing",
            FALSE, G_PARAM_READWRITE));

    g_object_interface_install_property(iface,
        g_param_spec_string("uri", "Uri", "Current uri",
            NULL, G_PARAM_READWRITE));

    g_object_interface_install_property(iface,
        g_param_spec_double("buffer-fill", "buffer-fill", "Current buffer fill",
            0, 1.0, 0, G_PARAM_READWRITE));

    g_object_interface_install_property(iface,
        g_param_spec_int64("duration", "Duration", "Current duration",
            0, G_MAXINT64, 0, G_PARAM_READABLE));

    g_object_interface_install_property(iface,
        g_param_spec_int64("position", "Position", "Current position",
            0, G_MAXINT64, 0, G_PARAM_READWRITE));

    g_object_interface_install_property(iface,
        g_param_spec_boolean("seekable", "Seekable", "Whether seekable",
            FALSE, G_PARAM_READABLE));
}

GtkWidget*
gt_player_backend_get_widget(GtPlayerBackend* backend)
{
    g_assert(GT_IS_PLAYER_BACKEND(backend));
    g_assert_nonnull(GT_PLAYER_BACKEND_GET_IFACE(backend)->get_widget);

    return GT_PLAYER_BACKEND_GET_IFACE(backend)->get_widget(backend);
}
