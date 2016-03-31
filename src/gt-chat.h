#ifndef GT_CHAT_H
#define GT_CHAT_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_CHAT gt_chat_get_type()

G_DECLARE_FINAL_TYPE(GtChat, gt_chat, GT, CHAT, GtkBox)

struct _GtChat
{
    GtkBox parent_instance;
};

GtChat* gt_chat_new();
void gt_chat_connect(GtChat* self, const gchar* chan);
void gt_chat_disconnect(GtChat* self);

G_END_DECLS

#endif
