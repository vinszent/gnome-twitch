#ifndef GT_FOLLOW_CHANNELS_VIEW_H
#define GT_FOLLOW_CHANNELS_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_FOLLOW_CHANNELS_VIEW (gt_follow_channels_view_get_type())

G_DECLARE_FINAL_TYPE(GtFollowChannelsView, gt_follow_channels_view, GT, FOLLOW_CHANNELS_VIEW, GtkBox)

struct _GtFollowChannelsView
{
    GtkBox parent_instance;
};

GtFollowChannelsView* gt_follow_channels_view_new(void);

G_END_DECLS

#endif
