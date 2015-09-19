#ifndef GT_CHANNELS_VIEW_CHILD_H
#define GT_CHANNELS_VIEW_CHILD_H

#include <gtk/gtk.h>
#include "gt-channel.h"

G_BEGIN_DECLS

#define GT_TYPE_CHANNELS_VIEW_CHILD (gt_channels_view_child_get_type())

G_DECLARE_FINAL_TYPE(GtChannelsViewChild, gt_channels_view_child, GT, CHANNELS_VIEW_CHILD, GtkFlowBoxChild)

struct _GtChannelsViewChild
{
    GtkFlowBoxChild parent_instance;
};

GtChannelsViewChild* gt_channels_view_child_new(GtChannel* chan);
void gt_channels_view_child_hide_overlay(GtChannelsViewChild* self);

G_END_DECLS

#endif
