#ifndef GT_FOLLOWS_VIEW_H
#define GT_FOLLOWS_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_FOLLOWS_VIEW (gt_follows_view_get_type())

G_DECLARE_FINAL_TYPE(GtFollowsView, gt_follows_view, GT, FOLLOWS_VIEW, GtkBox)

struct _GtFollowsView
{
    GtkBox parent_instance;
};

gboolean gt_follows_view_handle_event(GtFollowsView* self, GdkEvent* event);

G_END_DECLS

#endif
