#ifndef GT_SEARCH_GAME_CONTAINER_H
#define GT_SEARCH_GAME_CONTAINER_H

#include <glib-object.h>
#include "gt-item-container.h"

G_BEGIN_DECLS

#define GT_TYPE_SEARCH_GAME_CONTAINER gt_search_game_container_get_type()

G_DECLARE_FINAL_TYPE(GtSearchGameContainer, gt_search_game_container, GT, SEARCH_GAME_CONTAINER, GtItemContainer);

struct _GtSearchGameContainer
{
    GtItemContainer parent_instance;
};

GtSearchGameContainer* gt_search_game_container_new();

G_END_DECLS

#endif
