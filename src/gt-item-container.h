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

#ifndef GT_ITEM_CONTAINER_H
#define GT_ITEM_CONTAINER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_ITEM_CONTAINER (gt_item_container_get_type())

G_DECLARE_DERIVABLE_TYPE(GtItemContainer, gt_item_container, GT, ITEM_CONTAINER, GtkStack)

struct _GtItemContainerClass
{
    GtkStackClass parent_class;

    /* Protected funcs */
    void (*get_properties) (GtItemContainer* item_container, gint* child_width, gint* child_height, gboolean* append_extra,
        gchar** empty_label_text, gchar** empty_sub_label_text, gchar** empty_image_name,
        gchar** error_label_text, gchar** fetching_label_text);
    GTaskThreadFunc fetch_items;
    GtkWidget* (*create_child) (gpointer data);
    void (*activate_child) (GtItemContainer* item_container, gpointer child);
};

typedef struct
{
    gint amount;
    gint offset;
} FetchItemsData;

GtkWidget* gt_item_container_get_flow_box(GtItemContainer* self);
void gt_item_container_append_items(GtItemContainer* self, GList* items);
void gt_item_container_refresh(GtItemContainer* self);

G_END_DECLS;

#endif
