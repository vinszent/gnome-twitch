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

#ifndef GT_CONTAINER_VIEW_H
#define GT_CONTAINER_VIEW_H

#include <gtk/gtk.h>
#include "gt-item-container.h"

G_BEGIN_DECLS

#define GT_TYPE_CONTAINER_VIEW gt_container_view_get_type()

G_DECLARE_DERIVABLE_TYPE(GtContainerView, gt_container_view, GT, CONTAINER_VIEW, GtkBox);

struct _GtContainerViewClass
{
    GtkBoxClass parent_class;

    void (*refresh)(GtContainerView* self);
    void (*go_back)(GtContainerView* self);
};

void gt_container_view_add_container(GtContainerView* self, GtItemContainer* container);
void gt_container_view_set_search_active(GtContainerView* self, gboolean search_active);
gboolean gt_container_view_get_search_active(GtContainerView* self);
gboolean gt_container_view_get_show_back_button(GtContainerView* self);
GtkWidget* gt_container_view_get_container_stack(GtContainerView* self);
GtkWidget* gt_container_view_get_search_entry(GtContainerView* self);
GtkWidget* gt_container_view_get_search_bar(GtContainerView* self);
void gt_container_view_set_search_popover_widget(GtContainerView* self, GtkWidget* popover);
void gt_container_view_go_back(GtContainerView* self);
void gt_container_view_refresh(GtContainerView* self);

G_END_DECLS

#endif
