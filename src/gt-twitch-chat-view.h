
#ifndef GT_TWITCH_CHAT_VIEW_H
#define GT_TWITCH_CHAT_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_TWITCH_CHAT_VIEW gt_twitch_chat_view_get_type()

G_DECLARE_FINAL_TYPE(GtTwitchChatView, gt_twitch_chat_view, GT, TWITCH_CHAT_VIEW, GtkBox)

struct _GtTwitchChatView
{
    GtkBox parent_instance;
};

GtTwitchChatView* gt_twitch_chat_view_new();

G_END_DECLS

#endif
