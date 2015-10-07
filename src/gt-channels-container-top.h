#ifndef GT_CHANNELS_CONTAINER_TOP_H
#define GT_CHANNELS_CONTAINER_TOP_H

#include <gtk/gtk.h>
#include "gt-channels-container.h"

G_BEGIN_DECLS

#define GT_TYPE_CHANNELS_CONTAINER_TOP (gt_channels_container_top_get_type())

G_DECLARE_FINAL_TYPE(GtChannelsContainerTop, gt_channels_container_top, GT, CHANNELS_CONTAINER_TOP, GtChannelsContainer)

struct _GtChannelsContainerTop
{
    GtChannelsContainer parent_instance;
};

GtChannelsContainerTop* gt_channels_container_top_new(void);

G_END_DECLS

#endif
