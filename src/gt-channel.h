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

#ifndef GT_CHANNEL_H
#define GT_CHANNEL_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_CHANNEL (gt_channel_get_type())

G_DECLARE_FINAL_TYPE(GtChannel, gt_channel, GT, CHANNEL, GInitiallyUnowned)

struct _GtChannel
{
    GInitiallyUnowned parent_instance;
};

typedef struct
{
    gchar* id;
    gchar* game;
    gint64 viewers;
    GDateTime* stream_started_time;
    gchar* status;
    gchar* name;
    gchar* display_name;
    gchar* preview_url;
    gchar* video_banner_url;
    gchar* logo_url;
    gchar* profile_url;
    gboolean online;
} GtChannelData;

typedef GList GtChannelDataList;
typedef GList GtChannelList;

GtChannel*     gt_channel_new(GtChannelData* data);
GtChannel*     gt_channel_new_from_id_and_name(const gchar* id, const gchar* name);
void           gt_channel_toggle_followed(GtChannel* self);
void           gt_channel_list_free(GList* list);
gboolean       gt_channel_compare(GtChannel* self, gpointer other);
const gchar*   gt_channel_get_name(GtChannel* self);
const gchar*   gt_channel_get_display_name(GtChannel* self);
const gchar*   gt_channel_get_id(GtChannel* self);
const gchar*   gt_channel_get_game_name(GtChannel* self);
const gchar*   gt_channel_get_status(GtChannel* self);
gboolean       gt_channel_is_online(GtChannel* self);
gboolean       gt_channel_is_error(GtChannel* self);
gboolean       gt_channel_is_updating(GtChannel* self);
gboolean       gt_channel_is_followed(GtChannel* self);
const gchar*   gt_channel_get_error_message(GtChannel* self);
const gchar*   gt_channel_get_error_details(GtChannel* self);
gboolean       gt_channel_update(GtChannel* self);
GtChannelData* gt_channel_data_new();
void           gt_channel_data_free(GtChannelData* data);
void           gt_channel_data_list_free(GList* list);
gint           gt_channel_data_compare(GtChannelData* a, GtChannelData* b);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtChannelList, gt_channel_list_free);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtChannelData, gt_channel_data_free);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtChannelDataList, gt_channel_data_list_free);

G_END_DECLS

#endif
