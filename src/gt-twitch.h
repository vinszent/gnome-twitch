#ifndef GT_TWITCH_H
#define GT_TWITCH_H

#include <gtk/gtk.h>

#define MAX_QUERY 30
#define NO_GAME ""

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

typedef struct _GtChannelRawData
{
    gint64 id;
    gchar* game;
    gint64 viewers;
    GDateTime* stream_started_time;
    gchar* status;
    gchar* name;
    gchar* display_name;
    GdkPixbuf* preview;
    GdkPixbuf* video_banner;
    gboolean online;
} GtChannelRawData;

typedef struct _GtGameRawData
{
    gint64 id;
    gchar* name;
    GdkPixbuf* preview;
    GdkPixbuf* logo;
    gint64 viewers;
    gint64 channels;
} GtGameRawData;

GtTwitch*               gt_twitch_new(void);
void                    gt_twitch_stream_access_token(GtTwitch* self, gchar* channel, gchar** token, gchar** sig);
GtTwitchStreamData*     gt_twitch_stream_by_quality(GtTwitch* self, gchar* channel, GtTwitchStreamQuality qual, gchar* token, gchar* sig);
GList*                  gt_twitch_all_streams(GtTwitch* self, gchar* channel, gchar* token, gchar* sig);
GList*                  gt_twitch_top_channels(GtTwitch* self, gint n, gint offset, gchar* game);
GList*                  gt_twitch_top_channels_async(GtTwitch* self, gint n, gint offset, gchar* game, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
GList*                  gt_twitch_top_games(GtTwitch* self, gint n, gint offset);
GList*                  gt_twitch_top_games_async(GtTwitch* self, gint n, gint offset, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
GList*                  gt_twitch_search_channels(GtTwitch* self, gchar* query, gint n, gint offset);
void                    gt_twitch_search_channels_async(GtTwitch* self, gchar* query, gint n, gint offset, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
GList*                  gt_twitch_search_games(GtTwitch* self, gchar* query, gint n, gint offset);
void                    gt_twitch_search_games_async(GtTwitch* self, gchar* query, gint n, gint offset, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
void                    gt_twitch_stream_free(GtTwitchStreamData* channel);
GtChannelRawData* gt_twitch_channel_raw_data(GtTwitch* self, const gchar* name);
GtChannelRawData* gt_twitch_channel_with_stream_raw_data(GtTwitch* self, const gchar* name);
void                    gt_twitch_channel_raw_data_async(GtTwitch* self, const gchar* name, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
void                    gt_twitch_channel_raw_data_free(GtChannelRawData* data);
GtGameRawData*          gt_twitch_game_raw_data(GtTwitch* self, const gchar* name);
void                    gt_twitch_game_raw_data_free(GtGameRawData* data);

G_END_DECLS

#endif
