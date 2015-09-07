#ifndef GT_BROWSE_HEADER_BAR_H
#define GT_BROWSE_HEADER_BAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_BROWSE_HEADER_BAR (gt_browse_header_bar_get_type())

G_DECLARE_FINAL_TYPE(GtBrowseHeaderBar, gt_browse_header_bar, GT, BROWSE_HEADER_BAR, GtkHeaderBar)

struct _GtBrowseHeaderBar
{
    GtkHeaderBar parent_instance;
};

GtBrowseHeaderBar* gt_browse_header_bar_new(void);
void gt_browse_header_bar_stop_search(GtBrowseHeaderBar* self);

G_END_DECLS

#endif
