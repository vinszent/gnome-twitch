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

#define TAG "GtPlayerBackend"
#include "gnome-twitch/gt-log.h"

G_DEFINE_INTERFACE(GtPlayerBackend, gt_player_backend, G_TYPE_OBJECT)

static const GEnumValue gt_player_backend_state_enum_values[] =
{
    {GT_PLAYER_BACKEND_STATE_PLAYING, "GT_PLAYER_BACKEND_STATE_PLAYING", "playing"},
    {GT_PLAYER_BACKEND_STATE_PAUSED, "GT_PLAYER_BACKEND_STATE_PAUSED", "paused"},
    {GT_PLAYER_BACKEND_STATE_STOPPED, "GT_PLAYER_BACKEND_STATE_STOPPED", "stopped"},
    {GT_PLAYER_BACKEND_STATE_BUFFERING, "GT_PLAYER_STATE_BACKEND_BUFFERING", "buffering"},
    {GT_PLAYER_BACKEND_STATE_LOADING, "GT_PLAYER_STATE_BACKEND_LOADING", "loading"},
    {0, NULL, NULL},
};

GType
gt_player_backend_state_get_type()
{
    static GType type = 0;

    if (!type)
        type = g_enum_register_static("GtPlayerBackendState", gt_player_backend_state_enum_values);

    return type;
}

static void
gt_player_backend_default_init(GtPlayerBackendInterface* iface)
{
    g_object_interface_install_property(iface, g_param_spec_double("volume", "Volume", "Current volume",
            0, 1.0, 0.3, G_PARAM_READWRITE));

    g_object_interface_install_property(iface, g_param_spec_enum("state", "State", "Current state",
            GT_TYPE_PLAYER_BACKEND_STATE, GT_PLAYER_BACKEND_STATE_STOPPED, G_PARAM_READABLE));

    g_object_interface_install_property(iface, g_param_spec_double("buffer-fill", "buffer-fill", "Current buffer fill",
            0, 1.0, 0, G_PARAM_READABLE));

    g_object_interface_install_property(iface, g_param_spec_int64("duration", "Duration", "Current duration",
            0, G_MAXINT64, 0, G_PARAM_READABLE));

    g_object_interface_install_property(iface, g_param_spec_int64("position", "Position", "Current position",
            0, G_MAXINT64, 0, G_PARAM_READABLE));

    g_object_interface_install_property(iface, g_param_spec_boolean("seekable", "Seekable", "Whether seekable",
            FALSE, G_PARAM_READABLE));
}

GtkWidget*
gt_player_backend_get_widget(GtPlayerBackend* backend)
{
    g_assert(GT_IS_PLAYER_BACKEND(backend));
    g_assert_nonnull(GT_PLAYER_BACKEND_GET_IFACE(backend)->get_widget);

    return GT_PLAYER_BACKEND_GET_IFACE(backend)->get_widget(backend);
}

void
gt_player_backend_play(GtPlayerBackend* backend)
{
    RETURN_IF_FAIL(GT_IS_PLAYER_BACKEND(backend));

    GT_PLAYER_BACKEND_GET_IFACE(backend)->play(backend);
}

void
gt_player_backend_stop(GtPlayerBackend* backend)
{
    RETURN_IF_FAIL(GT_IS_PLAYER_BACKEND(backend));

    GT_PLAYER_BACKEND_GET_IFACE(backend)->stop(backend);
}

void
gt_player_backend_pause(GtPlayerBackend* backend)
{
    RETURN_IF_FAIL(GT_IS_PLAYER_BACKEND(backend));

    GT_PLAYER_BACKEND_GET_IFACE(backend)->pause(backend);
}

void
gt_player_backend_set_uri(GtPlayerBackend* backend, const gchar* uri)
{
    RETURN_IF_FAIL(GT_IS_PLAYER_BACKEND(backend));

    GT_PLAYER_BACKEND_GET_IFACE(backend)->set_uri(backend, uri);
}

void
gt_player_backend_set_position(GtPlayerBackend* backend, gint64 position)
{
    RETURN_IF_FAIL(GT_IS_PLAYER_BACKEND(backend));

    GT_PLAYER_BACKEND_GET_IFACE(backend)->set_position(backend, position);
}

GtPlayerBackendState
gt_player_backend_get_state(GtPlayerBackend* backend)
{
    RETURN_VAL_IF_FAIL(GT_IS_PLAYER_BACKEND(backend), GT_PLAYER_BACKEND_STATE_STOPPED);

    GtPlayerBackendState state;

    g_object_get(backend, "state", &state, NULL);

    return state;
}
