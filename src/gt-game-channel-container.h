#ifndef GT_GAME_CHANNEL_CONTAINER_H
#define GT_GAME_CHANNEL_CONTAINER_H

#include <glib-object.h>
#include "gt-item-container.h"

G_BEGIN_DECLS

#define GT_TYPE_GAME_CHANNEL_CONTAINER gt_game_channel_container_get_type()

G_DECLARE_FINAL_TYPE(GtGameChannelContainer, gt_game_channel_container, GT, GAME_CHANNEL_CONTAINER, GtItemContainer);

struct _GtGameChannelContainer
{
    GtItemContainer parent_instance;
};

GtGameChannelContainer* gt_game_channel_container_new();

G_END_DECLS

#endif
