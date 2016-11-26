#ifndef GT_TOP_CHANNEL_CONTAINER_H
#define GT_TOP_CHANNEL_CONTAINER_H

#include <glib-object.h>
#include "gt-item-container.h"

G_BEGIN_DECLS

#define GT_TYPE_TOP_CHANNEL_CONTAINER gt_top_channel_container_get_type()

G_DECLARE_FINAL_TYPE(GtTopChannelContainer, gt_top_channel_container, GT, TOP_CHANNEL_CONTAINER, GtItemContainer);

struct _GtTopChannelContainer
{
    GtItemContainer parent_instance;
};

GtTopChannelContainer* gt_top_channel_container_new();

G_END_DECLS

#endif
