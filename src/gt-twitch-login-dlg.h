
#ifndef GT_TWITCH_LOGIN_DLG_H
#define GT_TWITCH_LOGIN_DLG_H

#include <gtk/gtk.h>
#include "gt-win.h"

G_BEGIN_DECLS

#define GT_TYPE_TWITCH_LOGIN_DLG gt_twitch_login_dlg_get_type()

G_DECLARE_FINAL_TYPE(GtTwitchLoginDlg, gt_twitch_login_dlg, GT, TWITCH_LOGIN_DLG, GtkDialog)
    
struct _GtTwitchLoginDlg
{
    GtkDialog parent_instance;
};

GtTwitchLoginDlg* gt_twitch_login_dlg_new(GtWin* win);

G_END_DECLS

#endif
