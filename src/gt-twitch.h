#ifndef GT_TWITCH_H
#define GT_TWITCH_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_TWITCH (gt_twitch_get_type())

G_DECLARE_FINAL_TYPE(GtTwitch, gt_twitch, GT, TWITCH, GObject)

#include "gt-app.h"
#include "gt-channel.h"
#include "gt-game.h"

#define NO_GAME ""
#define NO_TIMESTAMP -1
#define ALL_LANGUAGES ""

typedef enum _GtTwitchError
{
    GT_TWITCH_ERROR_SOUP,
    GT_TWITCH_ERROR_JSON,
    GT_TWITCH_ERROR_MISC,
} GtTwitchError;

#define GT_TWITCH_STREAM_QUALITY_SOURCE "source"
#define GT_TWITCH_STREAM_QUALITY_HIGH "high"
#define GT_TWITCH_STREAM_QUALITY_MEDIUM "medium"
#define GT_TWITCH_STREAM_QUALITY_LOW "low"
#define GT_TWITCH_STREAM_QUALITY_MOBILE "mobile"
#define GT_TWITCH_STREAM_QUALITY_AUDIO_ONLY "audio_only"

struct _GtTwitch
{
    GObject parent_instance;
};

//TODO: Define an autofree for this
typedef struct _GtTwitchStreamAccessToken
{
    gchar* token;
    gchar* sig;
} GtTwitchStreamAccessToken;

//NOTE: Default qualities are source, high, medium, low, mobile,
//audio_only

typedef struct _GtTwitchStreamData
{
    gint width;
    gint height;
    glong bandwidth;
    gchar* quality;
    gchar* url;
} GtTwitchStreamData;


typedef struct
{
    gchar* name;
    gchar* version;
    GdkPixbuf* pixbuf;
} GtChatBadge;

typedef struct
{
    gint64 id;
    gchar* code;
    gint set;
    GdkPixbuf* pixbuf;

    //NOTE: These are only set if coming from GtIrc
    gint start; // Start of text to replace
    gint end; // End of text to replace
} GtChatEmote;

typedef enum _GtTwitchChannelInfoPanelType
{
    GT_TWITCH_CHANNEL_INFO_PANEL_TYPE_DEFAULT,
    GT_TWITCH_CHANNEL_INFO_PANEL_TYPE_TEESPRING,
} GtTwitchChannelInfoPanelType;

typedef struct _GtTwitchChannelInfoPanel
{
    GtTwitchChannelInfoPanelType type;
    gchar* title;
    gchar* html_description;
    gchar* markdown_description;
    GdkPixbuf* image;
    gchar* link;
    gint64 order;
} GtTwitchChannelInfoPanel;

GtTwitch*                  gt_twitch_new();
void                       gt_twitch_stream_access_token_free(GtTwitchStreamAccessToken* token);
GtTwitchStreamAccessToken* gt_twitch_stream_access_token(GtTwitch* self, const gchar* channel, GError** error);
void                       gt_twitch_stream_access_token_async(GtTwitch* self, const gchar* channel, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
const GtTwitchStreamData*        gt_twitch_stream_list_filter_quality(GList* list, const gchar* quality);
GList*                     gt_twitch_all_streams(GtTwitch* self, const gchar* channel, GError** error);
void                       gt_twitch_all_streams_async(GtTwitch* self, const gchar* channel, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
GList*                     gt_twitch_all_streams_finish(GtTwitch* self, GAsyncResult* result, GError** error);
GList*                     gt_twitch_top_channels(GtTwitch* self, gint n, gint offset, const gchar* game, const gchar* language, GError** error);
void                       gt_twitch_top_channels_async(GtTwitch* self, gint n, gint offset, const gchar* game, const gchar* language, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
GList*                     gt_twitch_top_games(GtTwitch* self, gint n, gint offset, GError** error);
void                       gt_twitch_top_games_async(GtTwitch* self, gint n, gint offset, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
GList*                     gt_twitch_top_games_finish(GtTwitch* self, GAsyncResult* result, GError** error);
GList*                     gt_twitch_search_channels(GtTwitch* self, const gchar* query, gint n, gint offset, gboolean offline, GError** error);
void                       gt_twitch_search_channels_async(GtTwitch* self, const gchar* query, gint n, gint offset, gboolean offline, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
GList*                     gt_twitch_search_channels_finish(GtTwitch* self, GAsyncResult* result, GError** error);
GList*                     gt_twitch_search_games(GtTwitch* self, const gchar* query, gint n, gint offset, GError** error);
void                       gt_twitch_search_games_async(GtTwitch* self, const gchar* query, gint n, gint offset, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
GList*                     gt_twitch_search_games_finish(GtTwitch* self, GAsyncResult* result, GError** error);
void                       gt_twitch_stream_data_free(GtTwitchStreamData* data);
void                       gt_twitch_stream_data_list_free(GList* list);
GtChannelData*             gt_twitch_fetch_channel_data(GtTwitch* self, const gchar* id, GError** error);
void                       gt_twitch_channel_raw_data_async(GtTwitch* self, const gchar* name, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
GtGameData*                gt_twitch_game_raw_data(GtTwitch* self, const gchar* name);
GdkPixbuf*                 gt_twitch_download_picture(GtTwitch* self, const gchar* url, gint64 timestamp, GError** error);
void                       gt_twitch_download_picture_async(GtTwitch* self, const gchar* url, gint64 timestamp, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
GdkPixbuf*                 gt_twitch_download_emote(GtTwitch* self, gint id);
GList*                     gt_twitch_channel_info(GtTwitch* self, const gchar* chan);
void                       gt_twitch_channel_info_panel_free(GtTwitchChannelInfoPanel* panel);
void                       gt_twitch_channel_info_async(GtTwitch* self, const gchar* chan, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
GList*                     gt_twitch_chat_servers(GtTwitch* self, const gchar* chan, GError** error);
GList*                     gt_twitch_fetch_all_followed_channels(GtTwitch* self, const gchar* id, const gchar* oauth_token, GError** error);
void                       gt_twitch_fetch_all_followed_channels_async(GtTwitch* self, const gchar* id, const gchar* oauth_token, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
GList*                     gt_twitch_fetch_all_followed_channels_finish(GtTwitch* self, GAsyncResult* result, GError** error);
void                       gt_twitch_follow_channel(GtTwitch* self, const gchar* chan_name, GError** error);
void                       gt_twitch_follow_channel_async(GtTwitch* self, const gchar* chan_name, GAsyncReadyCallback cb, gpointer udata);
void                       gt_twitch_follow_channel_finish(GtTwitch* self, GAsyncResult* result, GError** error);
void                       gt_twitch_unfollow_channel(GtTwitch* self, const gchar* chan_name, GError** error);
void                       gt_twitch_unfollow_channel_async(GtTwitch* self, const gchar* chan_name, GAsyncReadyCallback cb, gpointer udata);
void                       gt_twitch_unfollow_channel_finish(GtTwitch* self, GAsyncResult* result, GError** error);
GList*                     gt_twitch_emoticons(GtTwitch* self, const gchar* emotesets, GError** error);
void                       gt_twitch_emoticons_async(GtTwitch* self, const char* emotesets, GAsyncReadyCallback cb, GCancellable* cancel, gpointer udata);
GtChatBadge*               gt_twitch_fetch_chat_badge(GtTwitch* self, const gchar* chan_id, const gchar* badge_name, const gchar* version, GError** err);
void                       gt_twitch_fetch_chat_badge_async(GtTwitch* self, const gchar* chan_id, const gchar* badge_name, const gchar* version, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata);
GtChatBadge*               gt_twitch_fetch_chat_badge_finish(GtTwitch* self, GAsyncResult* result, GError** err);
void                       gt_twitch_load_chat_badge_sets_for_channel(GtTwitch* self, const gchar* chan_id, GError** err);
GtChatBadge*               gt_chat_badge_new();
void                       gt_chat_badge_free(GtChatBadge* badge);
void                       gt_chat_badge_list_free(GList* list);
GtChatEmote*               gt_chat_emote_new();
void                       gt_chat_emote_free(GtChatEmote* emote);
void                       gt_chat_emote_list_free(GList* list);
GtUserInfo*                gt_twitch_fetch_user_info(GtTwitch* self, const gchar* oauth_token, GError** error);
void                       gt_twitch_fetch_user_info_async(GtTwitch* self, const gchar* oauth_token, GAsyncReadyCallback cb, GCancellable* cancel, gpointer udata);
GtUserInfo*                gt_twitch_fetch_user_info_finish(GtTwitch* self, GAsyncResult* result, GError** error);

G_END_DECLS

#endif
