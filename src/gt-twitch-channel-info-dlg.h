#ifndef GT_TWITCH_CHANNEL_INFO_DLG_H
#define GT_TWITCH_CHANNEL_INFO_DLG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_TWITCH_CHANNEL_INFO_DLG gt_twitch_channel_info_dlg_get_type()

G_DECLARE_FINAL_TYPE(GtTwitchChannelInfoDlg, gt_twitch_channel_info_dlg, GT, TWITCH_CHANNEL_INFO_DLG, GtkDialog)

struct _GtTwitchChannelInfoDlg
{
    GtkDialog parent_instance;
};

GtTwitchChannelInfoDlg* gt_twitch_channel_info_dlg_new(GtkWindow* win);
void gt_twitch_channel_info_dlg_load_channel(GtTwitchChannelInfoDlg* self, const gchar* chan);

G_END_DECLS

#endif
