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

#ifndef GT_GAME_H
#define GT_GAME_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_GAME (gt_game_get_type())

G_DECLARE_FINAL_TYPE(GtGame, gt_game, GT, GAME, GInitiallyUnowned)

struct _GtGame
{
    GInitiallyUnowned parent_instance;
};

typedef struct
{
    gchar* id;
    gchar* name;
    gchar* preview_url;
    gchar* logo_url;
    gint64 viewers;
    gint64 channels;
} GtGameData;

GtGame*      gt_game_new(GtGameData* data);
void         gt_game_update_from_raw_data(GtGame* self, GtGameData* data);
void         gt_game_list_free(GList* self);
const gchar* gt_game_get_name(GtGame* self);
gboolean     gt_game_get_updating(GtGame* self);
gint64       gt_game_get_viewers(GtGame* self);
GtGameData*  gt_game_data_new();
void         gt_game_data_free(GtGameData* data);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtGameData, gt_game_data_free);

G_END_DECLS

#endif
