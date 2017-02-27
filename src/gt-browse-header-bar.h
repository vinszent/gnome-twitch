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

#ifndef GT_BROWSE_HEADER_BAR_H
#define GT_BROWSE_HEADER_BAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_BROWSE_HEADER_BAR (gt_browse_header_bar_get_type())

G_DECLARE_FINAL_TYPE(GtBrowseHeaderBar, gt_browse_header_bar, GT, BROWSE_HEADER_BAR, GtkHeaderBar)

struct _GtBrowseHeaderBar
{
    GtkHeaderBar parent_instance;
};

void gt_browse_header_bar_stop_search(GtBrowseHeaderBar* self);
void gt_browse_header_bar_toggle_search(GtBrowseHeaderBar* self);
gboolean gt_browse_header_bar_handle_event(GtBrowseHeaderBar* self, GdkEvent* event);

G_END_DECLS

#endif
