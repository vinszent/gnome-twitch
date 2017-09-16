#ifndef GT_CHANNEL_HEADER_BAR_H
#define GT_CHANNEL_HEADER_BAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_CHANNEL_HEADER_BAR gt_channel_header_bar_get_type()

G_DECLARE_FINAL_TYPE(GtChannelHeaderBar, gt_channel_header_bar, GT, CHANNEL_HEADER_BAR, GtkHeaderBar);

struct _GtChannelHeaderBar
{
    GtkHeaderBar parent_instance;
};

G_END_DECLS

#endif
