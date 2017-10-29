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

#ifndef GT_VOD_H
#define GT_VOD_H

#include "gt-channel.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define GT_TYPE_VOD gt_vod_get_type()

G_DECLARE_FINAL_TYPE(GtVOD, gt_vod, GT, VOD, GObject);

struct _GtVOD
{
    GObject parent_instance;
};

typedef struct
{
    gchar* id;
    gchar* broadcast_id;
    GDateTime* created_at;
    GDateTime* published_at;
    gchar* description;
    gchar* game;
    gchar* language;
    gint64 length;
    /* NOTE: Uncomment when needed */
    /* GtChannelData* channel; */
    struct
    {
        gchar* large;
        gchar* medium;
        gchar* small;
        gchar* template;
    } preview;
    gchar* title;
    gchar* url;
    gint64 views;
    gchar* tag_list;
} GtVODData;

GtVODData* gt_vod_data_new();
void gt_vod_data_free(GtVODData* data);
const gchar* gt_vod_get_id(GtVOD* self);
gboolean gt_vod_get_updating(GtVOD* self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtVODData, gt_vod_data_free);

GtVOD* gt_vod_new(GtVODData* data);

G_END_DECLS

#endif
