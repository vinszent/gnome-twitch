#ifndef GT_FAVOURITE_CHANNELS_VIEW_H
#define GT_FAVOURITE_CHANNELS_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_FAVOURITE_CHANNELS_VIEW (gt_favourite_channels_view_get_type())

G_DECLARE_FINAL_TYPE(GtFavouriteChannelsView, gt_favourite_channels_view, GT, FAVOURITE_CHANNELS_VIEW, GtkBox)

struct _GtFavouriteChannelsView
{
    GtkBox parent_instance;
};

GtFavouriteChannelsView* gt_favourite_channels_view_new(void);

G_END_DECLS

#endif
