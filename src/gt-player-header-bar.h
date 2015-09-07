#ifndef GT_PLAYER_HEADER_BAR_H
#define GT_PLAYER_HEADER_BAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_PLAYER_HEADER_BAR (gt_player_header_bar_get_type())

G_DECLARE_FINAL_TYPE(GtPlayerHeaderBar, gt_player_header_bar, GT, PLAYER_HEADER_BAR, GtkHeaderBar)

struct _GtPlayerHeaderBar
{
    GtkHeaderBar parent_instance;
};

GtPlayerHeaderBar* gt_player_header_bar_new(void);

G_END_DECLS

#endif
