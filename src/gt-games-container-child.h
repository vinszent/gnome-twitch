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

#ifndef GT_GAMES_CONTAINER_CHILD_H
#define GT_GAMES_CONTAINER_CHILD_H

#include <gtk/gtk.h>
#include "gt-game.h"

G_BEGIN_DECLS

#define GT_TYPE_GAMES_VIEW_CHILD (gt_games_container_child_get_type())

G_DECLARE_FINAL_TYPE(GtGamesContainerChild, gt_games_container_child, GT, GAMES_CONTAINER_CHILD, GtkFlowBoxChild)

struct _GtGamesContainerChild
{
    GtkFlowBoxChild parent_instance;
};

GtGamesContainerChild* gt_games_container_child_new(GtGame* game);
void gt_games_container_child_hide_overlay(GtGamesContainerChild* self);
GtGame* gt_games_container_child_get_game(GtGamesContainerChild* self);

G_END_DECLS

#endif
