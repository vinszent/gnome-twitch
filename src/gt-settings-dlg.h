#ifndef GT_SETTINGS_DLG_H
#define GT_SETTINGS_DLG_H

#include "gt-win.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_SETTINGS_DLG (gt_settings_dlg_get_type())

G_DECLARE_FINAL_TYPE(GtSettingsDlg, gt_settings_dlg, GT, SETTINGS_DLG, GtkDialog)

struct _GtSettingsDlg
{
    GtkDialog parent_instance;
};

GtSettingsDlg* gt_settings_dlg_new(GtWin* win);

G_END_DECLS

#endif
