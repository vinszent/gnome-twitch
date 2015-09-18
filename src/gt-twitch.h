#ifndef GT_TWITCH_H
#define GT_TWITCH_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_TWITCH (gt_twitch_get_type())

G_DECLARE_FINAL_TYPE(GtTwitch, gt_twitch, GT, TWITCH, GObject)

typedef enum _GtTwitchStreamQuality
{
    GT_TWITCH_STREAM_QUALITY_SOURCE,
    GT_TWITCH_STREAM_QUALITY_HIGH,
    GT_TWITCH_STREAM_QUALITY_MEDIUM,
    GT_TWITCH_STREAM_QUALITY_LOW,
    GT_TWITCH_STREAM_QUALITY_MOBILE,
    GT_TWITCH_STREAM_QUALITY_AUDIO_ONLY
} GtTwitchStreamQuality;

struct _GtTwitch
{
    GObject parent_instance;
};

typedef struct _GtTwitchStreamData
{
    gint width;
    gint height;
    glong bandwidth;
    GtTwitchStreamQuality quality;
    gchar* url;
} GtTwitchStreamData;

GtTwitch* gt_twitch_new(void);
void gt_twitch_stream_access_token(GtTwitch* self, gchar* channel, gchar** token, gchar** sig);
GtTwitchStreamData* gt_twitch_stream_by_quality(GtTwitch* self, gchar* channel, GtTwitchStreamQuality qual, gchar* token, gchar* sig);
GList* gt_twitch_all_streams(GtTwitch* self, gchar* channel, gchar* token, gchar* sig);
GList* gt_twitch_top_channels(GtTwitch* self, gint n, gint offset, gchar* game);
GList* gt_twitch_top_channels_async(GtTwitch* self, gint n, gint offset, gchar* game, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
GList* gt_twitch_top_games(GtTwitch* self, gint n, gint offset);
GList* gt_twitch_top_games_async(GtTwitch* self, gint n, gint offset, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
GList* gt_twitch_search_channels(GtTwitch* self, gchar* query, gint n, gint offset);
void gt_twitch_search_channels_async(GtTwitch* self, gchar* query, gint n, gint offset, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
GList* gt_twitch_search_games(GtTwitch* self, gchar* query, gint n, gint offset);
void gt_twitch_search_games_async(GtTwitch* self, gchar* query, gint n, gint offset, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
void gt_twitch_stream_free(GtTwitchStreamData* channel);

G_END_DECLS

#endif
