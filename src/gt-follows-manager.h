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

#ifndef GT_FOLLOWS_MANAGER_H
#define GT_FOLLOWS_MANAGER_H

#include <gtk/gtk.h>

#include "gt-channel.h"

G_BEGIN_DECLS

#define GT_TYPE_FOLLOWS_MANAGER (gt_follows_manager_get_type())

G_DECLARE_FINAL_TYPE(GtFollowsManager, gt_follows_manager, GT, FOLLOWS_MANAGER, GObject);

typedef struct _GtFollowsManagerPrivate GtFollowsManagerPrivate;

struct _GtFollowsManager
{
    GObject parent_instance;

    GtFollowsManagerPrivate* priv;

    GList* follow_channels;
};

GtFollowsManager* gt_follows_manager_new(void);
void gt_follows_manager_load_from_file(GtFollowsManager* self);
void gt_follows_manager_load_from_twitch(GtFollowsManager* self);
void gt_follows_manager_save(GtFollowsManager* self);
gboolean gt_follows_manager_is_channel_followed(GtFollowsManager* self, GtChannel* chan);
void gt_follows_manager_attach_to_channel(GtFollowsManager* self, GtChannel* chan);


G_END_DECLS

#endif
