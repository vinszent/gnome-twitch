#ifndef GT_FOLLOWED_CHANNEL_CONTAINER_H
#define GT_FOLLOWED_CHANNEL_CONTAINER_H

#include <glib-object.h>
#include "gt-item-container.h"

G_BEGIN_DECLS

#define GT_TYPE_FOLLOWED_CHANNEL_CONTAINER gt_followed_channel_container_get_type()

G_DECLARE_FINAL_TYPE(GtFollowedChannelContainer, gt_followed_channel_container, GT, FOLLOWED_CHANNEL_CONTAINER, GtItemContainer);

struct _GtFollowedChannelContainer
{
    GtItemContainer parent_instance;
};

GtFollowedChannelContainer* gt_followed_channel_container_new();

G_END_DECLS

#endif
