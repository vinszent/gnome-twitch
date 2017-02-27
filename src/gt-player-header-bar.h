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

#ifndef GT_PLAYER_HEADER_BAR_H
#define GT_PLAYER_HEADER_BAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_PLAYER_HEADER_BAR (gt_player_header_bar_get_type())

G_DECLARE_FINAL_TYPE(GtPlayerHeaderBar, gt_player_header_bar, GT, PLAYER_HEADER_BAR, GtkHeaderBar)

struct _GtPlayerHeaderBar
{
    GtkHeaderBar parent_instance;
};

GtPlayerHeaderBar* gt_player_header_bar_new(void);

G_END_DECLS

#endif
