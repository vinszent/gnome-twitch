#ifndef GT_STREAMS_VIEW_CHILD_H
#define GT_STREAMS_VIEW_CHILD_H

#include <gtk/gtk.h>
#include "gt-twitch-stream.h"

G_BEGIN_DECLS

#define GT_TYPE_STREAMS_VIEW_CHILD (gt_streams_view_child_get_type())

G_DECLARE_FINAL_TYPE(GtStreamsViewChild, gt_streams_view_child, GT, STREAMS_VIEW_CHILD, GtkFlowBoxChild)

struct _GtStreamsViewChild
{
    GtkFlowBoxChild parent_instance;
};

GtStreamsViewChild* gt_streams_view_child_new(GtTwitchStream* chan);
void gt_streams_view_child_hide_overlay(GtStreamsViewChild* self);

G_END_DECLS

#endif
