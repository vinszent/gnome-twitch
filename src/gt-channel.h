#ifndef GT_CHANNEL_H
#define GT_CHANNEL_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_CHANNEL (gt_channel_get_type())

G_DECLARE_FINAL_TYPE(GtChannel, gt_channel, GT, CHANNEL, GInitiallyUnowned)

struct _GtChannel
{
    GInitiallyUnowned parent_instance;
};

typedef struct
{
    gint64 id;
    gchar* game;
    gint64 viewers;
    GDateTime* stream_started_time;
    gchar* status;
    gchar* name;
    gchar* display_name;
    gchar* preview_url;
    gchar* video_banner_url;
    gchar* logo_url;
    gchar* profile_url;
    gboolean online;
} GtChannelData;

GtChannel*     gt_channel_new(const gchar* name, gint64 id);
void           gt_channel_update_from_raw_data(GtChannel* self, GtChannelData* data);
void           gt_channel_toggle_followed(GtChannel* self);
void           gt_channel_list_free(GList* list);
gboolean       gt_channel_compare(GtChannel* self, gpointer other);
const gchar*   gt_channel_get_name(GtChannel* self);
gint64         gt_channel_get_id(GtChannel* self);
gboolean       gt_channel_update(GtChannel* self);
GtChannelData* gt_channel_data_new();
void           gt_channel_data_free(GtChannelData* data);
void           gt_channel_data_list_free(GList* list);
gint           gt_channel_data_compare(GtChannelData* a, GtChannelData* b);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtChannelData, gt_channel_data_free);

G_END_DECLS

#endif
