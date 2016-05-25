#ifndef GT_CHANNEL_H
#define GT_CHANNEL_H

#include <gtk/gtk.h>
#include "gt-twitch.h"

G_BEGIN_DECLS

#define GT_TYPE_CHANNEL (gt_channel_get_type())

G_DECLARE_FINAL_TYPE(GtChannel, gt_channel, GT, CHANNEL, GObject)

struct _GtChannel
{
    GObject parent_instance;
};

GtChannel* gt_channel_new(const gchar* name, gint64 id);
void gt_channel_update_from_raw_data(GtChannel* self, GtChannelRawData* data);
void gt_channel_toggle_favourited(GtChannel* self);
void gt_channel_free_list(GList* list);
gboolean gt_channel_compare(GtChannel* self, gpointer other);
const gchar* gt_channel_get_name(GtChannel* self);
gboolean gt_channel_update(GtChannel* self);

G_END_DECLS

#endif
