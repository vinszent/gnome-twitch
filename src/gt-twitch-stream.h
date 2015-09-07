#ifndef GT_TWITCH_STREAM_H
#define GT_TWITCH_STREAM_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_TWITCH_STREAM (gt_twitch_stream_get_type())

G_DECLARE_FINAL_TYPE(GtTwitchStream, gt_twitch_stream, GT, TWITCH_STREAM, GObject)

struct _GtTwitchStream
{
    GObject parent_instance;
};

GtTwitchStream* gt_twitch_stream_new(gint64 id);
void gt_twitch_stream_free_list(GList* list);

G_END_DECLS

#endif
