#ifndef GT_TWITCH_CHANNEL_H
#define GT_TWITCH_CHANNEL_H

#include <gtk/gtk.h>
#include "gt-twitch.h"

G_BEGIN_DECLS

#define GT_TYPE_TWITCH_CHANNEL (gt_twitch_channel_get_type())

G_DECLARE_FINAL_TYPE(GtTwitchChannel, gt_twitch_channel, GT, TWITCH_CHANNEL, GObject)

struct _GtTwitchChannel
{
    GObject parent_instance;
};

GtTwitchChannel* gt_twitch_channel_new(gint64 id);
void gt_twitch_channel_update_from_raw_data(GtTwitchChannel* self, GtTwitchChannelRawData* data);
void gt_twitch_channel_toggle_favourited(GtTwitchChannel* self);
void gt_twitch_channel_free_list(GList* list);
gboolean gt_twitch_channel_compare(GtTwitchChannel* self, gpointer other);

G_END_DECLS

#endif
