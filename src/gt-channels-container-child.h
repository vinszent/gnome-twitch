#ifndef GT_CHANNELS_CONTAINER_CHILD_H
#define GT_CHANNELS_CONTAINER_CHILD_H

#include <gtk/gtk.h>
#include "gt-channel.h"

G_BEGIN_DECLS

#define GT_TYPE_CHANNELS_CONTAINER_CHILD (gt_channels_container_child_get_type())

G_DECLARE_FINAL_TYPE(GtChannelsContainerChild, gt_channels_container_child, GT, CHANNELS_CONTAINER_CHILD, GtkFlowBoxChild)

struct _GtChannelsContainerChild
{
    GtkFlowBoxChild parent_instance;

    GtChannel* channel;
};

GtChannelsContainerChild* gt_channels_container_child_new(GtChannel* chan);
void gt_channels_container_child_hide_overlay(GtChannelsContainerChild* self);

G_END_DECLS

#endif
