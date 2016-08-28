#ifndef GT_CHANNELS_CONTAINER_FOLLOW_H
#define GT_CHANNELS_CONTAINER_FOLLOW_H

#include <gtk/gtk.h>
#include "gt-channels-container.h"

G_BEGIN_DECLS

#define GT_TYPE_CHANNELS_CONTAINER_FOLLOW (gt_channels_container_follow_get_type())

G_DECLARE_FINAL_TYPE(GtChannelsContainerFollow, gt_channels_container_follow, GT, CHANNELS_CONTAINER_FOLLOW, GtChannelsContainer)

struct _GtChannelsContainerFollow
{
    GtChannelsContainer parent_instance;
};

GtChannelsContainerFollow* gt_channels_container_follow_new(void);

G_END_DECLS

#endif
