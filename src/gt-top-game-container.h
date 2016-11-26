#ifndef GT_TOP_GAME_CONTAINER_H
#define GT_TOP_GAME_CONTAINER_H

#include <glib-object.h>
#include "gt-item-container.h"

G_BEGIN_DECLS

#define GT_TYPE_TOP_GAME_CONTAINER gt_top_game_container_get_type()

G_DECLARE_FINAL_TYPE(GtTopGameContainer, gt_top_game_container, GT, TOP_GAME_CONTAINER, GtItemContainer);

struct _GtTopGameContainer
{
    GtItemContainer parent_instance;
};

GtTopGameContainer* gt_top_game_container_new();

G_END_DECLS

#endif
