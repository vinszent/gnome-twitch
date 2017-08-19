#ifndef GT_CHANNEL_VOD_CONTAINER_H
#define GT_CHANNEL_VOD_CONTAINER_H

#include "gt-item-container.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define GT_TYPE_CHANNEL_VOD_CONTAINER gt_channel_vod_container_get_type()

G_DECLARE_FINAL_TYPE(GtChannelVODContainer, gt_channel_vod_container, GT, CHANNEL_VOD_CONTAINER, GtItemContainer);

struct _GtChannelVODContainer
{
    GtItemContainer parent_instance;
};

G_END_DECLS

#endif
