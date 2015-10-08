#ifndef GT_CHANNELS_CONTAINER_FAVOURITE_H
#define GT_CHANNELS_CONTAINER_FAVOURITE_H

#include <gtk/gtk.h>
#include "gt-channels-container.h"

G_BEGIN_DECLS

#define GT_TYPE_CHANNELS_CONTAINER_FAVOURITE (gt_channels_container_favourite_get_type())

G_DECLARE_FINAL_TYPE(GtChannelsContainerFavourite, gt_channels_container_favourite, GT, CHANNELS_CONTAINER_FAVOURITE, GtChannelsContainer)

struct _GtChannelsContainerFavourite
{
    GtChannelsContainer parent_instance;
};

GtChannelsContainerFavourite* gt_channels_container_favourite_new(void);

G_END_DECLS

#endif
