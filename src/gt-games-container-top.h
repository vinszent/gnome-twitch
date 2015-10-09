#ifndef GT_GAMES_CONTAINER_TOP_H
#define GT_GAMES_CONTAINER_TOP_H

#include <gtk/gtk.h>
#include "gt-games-container.h"

G_BEGIN_DECLS

#define GT_TYPE_GAMES_CONTAINER_TOP (gt_games_container_top_get_type())

G_DECLARE_FINAL_TYPE(GtGamesContainerTop, gt_games_container_top, GT, GAMES_CONTAINER_TOP, GtGamesContainer)

struct _GtGamesContainerTop
{
    GtGamesContainer parent_instance;
};

GtGamesContainerTop* gt_games_container_top_new(void);

G_END_DECLS

#endif
