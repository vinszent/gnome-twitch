#ifndef GT_PLAYER_BACKEND_H
#define GT_PLAYER_BACKEND_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_PLAYER_BACKEND (gt_player_backend_get_type())

G_DECLARE_INTERFACE(GtPlayerBackend, gt_player_backend, GT, PLAYER_BACKEND, GObject);

struct _GtPlayerBackendInterface
{
    GTypeInterface parent_iface;

    GtkWidget* (*get_widget) (GtPlayerBackend* backend);
};

GtkWidget* gt_player_backend_get_widget(GtPlayerBackend* backend);

G_END_DECLS

#endif
