#ifndef GT_GAMES_VIEW_CHILD_H
#define GT_GAMES_VIEW_CHILD_H

#include <gtk/gtk.h>
#include "gt-twitch-game.h"

G_BEGIN_DECLS

#define GT_TYPE_GAMES_VIEW_CHILD (gt_games_view_child_get_type())

G_DECLARE_FINAL_TYPE(GtGamesViewChild, gt_games_view_child, GT, GAMES_VIEW_CHILD, GtkFlowBoxChild)

struct _GtGamesViewChild
{
    GtkFlowBoxChild parent_instance;
};

GtGamesViewChild* gt_games_view_child_new(GtTwitchGame* game);
void gt_games_view_child_hide_overlay(GtGamesViewChild* self);

G_END_DECLS

#endif
