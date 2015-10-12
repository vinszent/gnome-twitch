#ifndef GT_CHANNELS_CONTAINER_SEARCH_H
#define GT_CHANNELS_CONTAINER_SEARCH_H

#include <gtk/gtk.h>
#include "gt-channels-container.h"

G_BEGIN_DECLS

#define GT_TYPE_CHANNELS_CONTAINER_SEARCH (gt_channels_container_search_get_type())

G_DECLARE_FINAL_TYPE(GtChannelsContainerSearch, gt_channels_container_search, GT, CHANNELS_CONTAINER_SEARCH, GtChannelsContainer)

struct _GtChannelsContainerSearch
{
    GtChannelsContainer parent_instance;
};

GtChannelsContainerSearch* gt_channels_container_search_new(void);

G_END_DECLS

#endif
