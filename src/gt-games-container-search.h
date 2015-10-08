#ifndef GT_GAMES_CONTAINER_SEARCH_H
#define GT_GAMES_CONTAINER_SEARCH_H

#include <gtk/gtk.h>
#include "gt-games-container.h"

G_BEGIN_DECLS

#define GT_TYPE_GAMES_CONTAINER_SEARCH (gt_games_container_search_get_type())

G_DECLARE_FINAL_TYPE(GtGamesContainerSearch, gt_games_container_search, GT, GAMES_CONTAINER_SEARCH, GtGamesContainer)

struct _GtGamesContainerSearch
{
     GtGamesContainer parent_instance;
};

GtGamesContainerSearch* gt_games_container_search_new(void);

G_END_DECLS

#endif
