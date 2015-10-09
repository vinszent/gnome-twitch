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

G_END_DECLS

#endif
