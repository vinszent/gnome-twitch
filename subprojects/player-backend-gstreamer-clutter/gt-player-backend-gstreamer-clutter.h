#ifndef GT_PLAYER_BACKEND_GSTREAMER_CLUTTER_H
#define GT_PLAYER_BACKEND_GSTREAMER_CLUTTER_H

#include <gtk/gtk.h>
#include <libpeas/peas.h>

G_BEGIN_DECLS

#define GT_TYPE_PLAYER_BACKEND_GSTREAMER_CLUTTER gt_player_backend_gstreamer_clutter_get_type()

G_DECLARE_FINAL_TYPE(GtPlayerBackendGstreamerClutter, gt_player_backend_gstreamer_clutter, GT, PLAYER_BACKEND_GSTREAMER_CLUTTER, PeasExtensionBase);

struct _GtPlayerBackendGstreamerClutter
{
    PeasExtensionBase parent_instance;
};

G_MODULE_EXPORT void peas_register_types(PeasObjectModule* module);

G_END_DECLS

#endif
