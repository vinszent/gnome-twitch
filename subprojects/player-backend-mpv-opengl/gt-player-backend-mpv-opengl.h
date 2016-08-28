#ifndef GT_PLAYER_BACKEND_MPV_OPENGL_H
#define GT_PLAYER_BACKEND_MPV_OPENGL_H

#include <gtk/gtk.h>
#include <libpeas/peas.h>

G_BEGIN_DECLS

#define GT_TYPE_PLAYER_BACKEND_MPV_OPENGL gt_player_backend_mpv_opengl_get_type()

G_DECLARE_FINAL_TYPE(GtPlayerBackendMpvOpenGL, gt_player_backend_mpv_opengl, GT, PLAYER_BACKEND_MPV_OPENGL, PeasExtensionBase);

struct _GtPlayerBackendMpvOpenGL
{
    PeasExtensionBase parent_instance;
};

G_MODULE_EXPORT void peas_register_types(PeasObjectModule* module);

G_END_DECLS

#endif
