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

#ifndef GT_PLAYER_H
#define GT_PLAYER_H

#include "gt-channel.h"
#include "gt-vod.h"
#include "gt-twitch.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_PLAYER (gt_player_get_type())

G_DECLARE_FINAL_TYPE(GtPlayer, gt_player, GT, PLAYER, GtkStack)

struct _GtPlayer
{
    GtkStack parent_instance;
};

typedef struct
{
    gboolean visible;
    gboolean docked;
    gboolean dark_theme;
    gdouble opacity;
    gdouble width;
    gdouble height;
    gdouble x_pos;
    gdouble y_pos;
    gdouble docked_handle_pos;
} GtPlayerChannelSettings;

void                     gt_player_open_channel(GtPlayer* self, GtChannel* chan);
void                     gt_player_open_vod(GtPlayer* self, GtVOD* vod);
void                     gt_player_close_channel(GtPlayer* self);
void                     gt_player_play(GtPlayer* self);
void                     gt_player_stop(GtPlayer* self);
void                     gt_player_set_quality(GtPlayer* self, const gchar* quality);
void                     gt_player_toggle_muted(GtPlayer* self);
GtChannel*               gt_player_get_channel(GtPlayer* self);
GList*                   gt_player_get_available_stream_qualities(GtPlayer* self);
gboolean                 gt_player_is_playing(GtPlayer* self);
GtPlayerChannelSettings* gt_player_channel_settings_new();
void                     gt_player_channel_settings_free(GtPlayerChannelSettings* settings);

G_END_DECLS

#endif
