#ifndef GT_PLAYER_BACKEND_GSTREAMER_CAIRO_H
#define GT_PLAYER_BACKEND_GSTREAMER_CAIRO_H

#include <gtk/gtk.h>
#include <libpeas/peas.h>

G_BEGIN_DECLS

#define GT_TYPE_PLAYER_BACKEND_GSTREAMER_CAIRO gt_player_backend_gstreamer_cairo_get_type()

G_DECLARE_FINAL_TYPE(GtPlayerBackendGstreamerCairo, gt_player_backend_gstreamer_cairo, GT, PLAYER_BACKEND_GSTREAMER_CAIRO, PeasExtensionBase);

struct _GtPlayerBackendGstreamerCairo
{
    PeasExtensionBase parent_instance;
};

G_MODULE_EXPORT void peas_register_types(PeasObjectModule* module);

G_END_DECLS

#endif
