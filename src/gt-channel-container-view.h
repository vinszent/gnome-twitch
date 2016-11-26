#ifndef GT_CHANNEL_CONTAINER_VIEW_H
#define GT_CHANNEL_CONTAINER_VIEW_H

#include "gt-container-view.h"

G_BEGIN_DECLS

#define GT_TYPE_CHANNEL_CONTAINER_VIEW gt_channel_container_view_get_type()

G_DECLARE_FINAL_TYPE(GtChannelContainerView, gt_channel_container_view, GT, CHANNEL_CONTAINER_VIEW, GtContainerView);

struct _GtChannelContainerView
{
    GtContainerView parent_instance;
};

G_END_DECLS

#endif
