#ifndef GT_PLAYER_BACKEND_GSTREAMER_OPENGL_H
#define GT_PLAYER_BACKEND_GSTREAMER_OPENGL_H

#include <gtk/gtk.h>
#include <libpeas/peas.h>

G_BEGIN_DECLS

#define GT_TYPE_PLAYER_BACKEND_GSTREAMER_OPENGL gt_player_backend_gstreamer_opengl_get_type()

G_DECLARE_FINAL_TYPE(GtPlayerBackendGstreamerOpenGL, gt_player_backend_gstreamer_opengl, GT, PLAYER_BACKEND_GSTREAMER_OPENGL, PeasExtensionBase);

struct _GtPlayerBackendGstreamerOpenGL
{
    PeasExtensionBase parent_instance;
};

G_MODULE_EXPORT void peas_register_types(PeasObjectModule* module);

G_END_DECLS

#endif
