#ifndef GT_FAVOURITES_VIEW_H
#define GT_FAVOURITES_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_FAVOURITES_VIEW (gt_favourites_view_get_type())

G_DECLARE_FINAL_TYPE(GtFavouritesView, gt_favourites_view, GT, FAVOURITES_VIEW, GtkBox)

struct _GtFavouritesView
{
    GtkBox parent_instance;
};

gboolean gt_favourites_view_handle_event(GtFavouritesView* self, GdkEvent* event);

G_END_DECLS

#endif
