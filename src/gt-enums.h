#ifndef _GT_ENUMS_H
#define _GT_ENUMS_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_TWITCH_STREAM_QUALITY gt_twitch_stream_quality_get_type()
#define GT_TYPE_SETTINGS_DLG_VIEW gt_settings_dlg_view_get_type()

GType gt_twitch_stream_quality_get_type();
GType gt_settings_dlg_view_get_type();

G_END_DECLS

#endif
