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

#ifndef GT_PLAYER_BACKEND_H
#define GT_PLAYER_BACKEND_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_PLAYER_BACKEND (gt_player_backend_get_type())

G_DECLARE_INTERFACE(GtPlayerBackend, gt_player_backend, GT, PLAYER_BACKEND, GObject)

struct _GtPlayerBackendInterface
{
    GTypeInterface parent_iface;

    GtkWidget* (*get_widget) (GtPlayerBackend* backend);
};

/**
 * gt_player_backend_get_widget
 *
 * Returns: (transfer none): The Gtk Widget
 *
 * Returns a Gtk.Widget that presents the player to the user
 */
GtkWidget* gt_player_backend_get_widget(GtPlayerBackend* backend);

G_END_DECLS

#endif
