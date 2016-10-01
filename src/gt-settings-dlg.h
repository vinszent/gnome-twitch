#ifndef GT_SETTINGS_DLG_H
#define GT_SETTINGS_DLG_H

#include "gt-win.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_SETTINGS_DLG (gt_settings_dlg_get_type())

G_DECLARE_FINAL_TYPE(GtSettingsDlg, gt_settings_dlg, GT, SETTINGS_DLG, GtkDialog);

typedef enum _GtSettingsDlgView
{
    GT_SETTINGS_DLG_VIEW_GENERAL,
    GT_SETTINGS_DLG_VIEW_PLAYERS
} GtSettingsDlgView;

struct _GtSettingsDlg
{
    GtkDialog parent_instance;
};

GtSettingsDlg* gt_settings_dlg_new(GtWin* win);
void gt_settings_dlg_set_view(GtSettingsDlg* self, GtSettingsDlgView view);

G_END_DECLS

#endif
