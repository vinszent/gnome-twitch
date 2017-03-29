/*
 *  This file is part of GNOME Twitch - 'Enjoy Twitch on your GNU/Linux desktop'
 *  Copyright © 2017 Vincent Szolnoky <vinszent@vinszent.com>
 *
 *  GNOME Twitch is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GNOME Twitch is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNOME Twitch. If not, see <http://www.gnu.org/licenses/>.
 */

#include "gt-twitch.h"
#include "gt-resource-downloader.h"
#include "config.h"
#include <libsoup/soup.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <glib.h>
#include <json-glib/json-glib.h>
#include <string.h>
#include <stdlib.h>
#include "utils.h"

#define TAG "GtTwitch"
#include "gnome-twitch/gt-log.h"

//TODO: Use https://streams.twitch.tv/kraken/streams/{channel}?stream_type=all instead to get is_playlist info
//TODO: Use https://tmi.twitch.tv/servers?channel=%s to get chat server info

#define ACCESS_TOKEN_URI       "http://api.twitch.tv/api/channels/%s/access_token"
#define STREAM_PLAYLIST_URI    "http://usher.twitch.tv/api/channel/hls/%s.m3u8?player=twitchweb&token=%s&sig=%s&allow_audio_only=true&allow_source=true&type=any&allow_spectre=true&p=%d"
#define TOP_CHANNELS_URI       "https://api.twitch.tv/kraken/streams?limit=%d&offset=%d&game=%s&language=%s"
#define TOP_GAMES_URI          "https://api.twitch.tv/kraken/games/top?limit=%d&offset=%d"
#define SEARCH_STREAMS_URI     "https://api.twitch.tv/kraken/search/streams?query=%s&limit=%d&offset=%d"
#define SEARCH_CHANNELS_URI    "https://api.twitch.tv/kraken/search/channels?query=%s&limit=%d&offset=%d"
#define SEARCH_GAMES_URI       "https://api.twitch.tv/kraken/search/games?query=%s&type=suggest"
#define FETCH_STREAM_URI       "https://api.twitch.tv/kraken/streams/%s"
#define FETCH_CHANNEL_URI      "https://api.twitch.tv/kraken/channels/%s"
#define CHAT_BADGES_URI        "https://api.twitch.tv/kraken/chat/%s/badges/"
#define TWITCH_EMOTE_URI       "https://static-cdn.jtvnw.net/emoticons/v1/%d/%d.0"
#define CHANNEL_INFO_URI       "http://api.twitch.tv/api/channels/%s/panels"
#define CHAT_SERVERS_URI       "https://api.twitch.tv/api/channels/%s/chat_properties"
#define FOLLOWED_STREAMS_URI   "https://api.twitch.tv/kraken/streams/followed?limit=%d&offset=%d&oauth_token=%s&stream_type=live"
#define FOLLOWED_CHANNELS_URI  "https://api.twitch.tv/kraken/users/%s/follows/channels?limit=%d&offset=%d"
#define FOLLOW_CHANNEL_URI     "https://api.twitch.tv/kraken/users/%s/follows/channels/%s?oauth_token=%s"
#define UNFOLLOW_CHANNEL_URI   "https://api.twitch.tv/kraken/users/%s/follows/channels/%s?oauth_token=%s"
#define USER_EMOTICONS_URI     "https://api.twitch.tv/kraken/users/%s/emotes"
#define EMOTICON_IMAGES_URI    "https://api.twitch.tv/kraken/chat/emoticon_images?emotesets=%s"
#define USER_INFO_URI          "https://api.twitch.tv/kraken/user?oauth_token=%s"
#define GLOBAL_CHAT_BADGES_URI "https://badges.twitch.tv/v1/badges/global/display"
#define NEW_CHAT_BADGES_URI    "https://badges.twitch.tv/v1/badges/channels/%s/display"
#define OAUTH_INFO_URI         "https://api.twitch.tv/kraken/?oauth_token=%s"

#define STREAM_INFO "#EXT-X-STREAM-INF"

#define TWITCH_API_VERSION_3 "3"
#define TWITCH_API_VERSION_4 "4"
#define TWITCH_API_VERSION_5 "5"

#define END_JSON_MEMBER() json_reader_end_member(reader) // Just for consistency's sake
#define END_JSON_ELEMENT() json_reader_end_element(reader) // Just for consistency's sake

#define READ_JSON_MEMBER(name) \
    if (!json_reader_read_member(reader, name))                         \
    {                                                                   \
        const GError* e = json_reader_get_error(reader);                \
                                                                        \
        g_set_error(error, GT_TWITCH_ERROR, GT_TWITCH_ERROR_JSON,       \
            "Unable to read JSON member with name '%s' because: %s", name, e->message); \
                                                                        \
        WARNINGF("Unable to read JSON member with name '%s' because: %s", name, e->message);   \
                                                                        \
        goto error;                                                     \
    }                                                                   \

#define READ_JSON_ELEMENT(i)                                            \
    if (!json_reader_read_element(reader, i))                           \
    {                                                                   \
        const GError* e = json_reader_get_error(reader);                \
                                                                        \
        g_set_error(error, GT_TWITCH_ERROR, GT_TWITCH_ERROR_JSON,       \
            "Unable to read JSON element with position '%d' because: %s", i, e->message); \
                                                                        \
        WARNINGF("Unable to read JSON element with position '%d' because: %s", i, e->message); \
                                                                        \
        goto error;                                                     \
    }                                                                   \

#define READ_JSON_ELEMENT_VALUE(i, p)                                   \
    if (json_reader_read_element(reader, i))                            \
    {                                                                   \
        p = _Generic(p,                                                 \
            gchar*: g_strdup(json_reader_get_string_value(reader)),     \
            gint64: json_reader_get_int_value(reader),                  \
            gdouble: json_reader_get_double_value(reader),              \
            gboolean: json_reader_get_boolean_value(reader),            \
            GDateTime*: parse_time(json_reader_get_string_value(reader))); \
    }                                                                   \
    else                                                                \
    {                                                                   \
        const GError* e = json_reader_get_error(reader);                \
                                                                        \
        g_set_error(error, GT_TWITCH_ERROR, GT_TWITCH_ERROR_JSON,       \
            "Unable to read JSON element with position '%d' because: %s", i, e->message); \
                                                                        \
        WARNINGF("Unable to read JSON element with position '%d' because: %s", i, e->message); \
                                                                        \
        goto error;                                                     \
    }                                                                   \
    json_reader_end_element(reader);                                    \


#define READ_JSON_VALUE(name, p)                                        \
    if (json_reader_read_member(reader, name))                          \
    {                                                                   \
        p = _Generic(p,                                                 \
            gchar*: g_strdup(json_reader_get_string_value(reader)),     \
            gint64: json_reader_get_int_value(reader),                  \
            gdouble: json_reader_get_double_value(reader),              \
            gboolean: json_reader_get_boolean_value(reader),            \
            GDateTime*: parse_time(json_reader_get_string_value(reader))); \
    }                                                                   \
    else                                                                \
    {                                                                   \
        const GError* e = json_reader_get_error(reader);                \
                                                                        \
        g_set_error(error, GT_TWITCH_ERROR, GT_TWITCH_ERROR_JSON,       \
            "Unable to read JSON member with name '%s' because: %s", name, e->message); \
                                                                        \
        WARNINGF("Unable to read JSON member with name '%s' because: %s", name, e->message); \
                                                                        \
        goto error;                                                     \
    }                                                                   \
    json_reader_end_member(reader);                                     \

#define READ_JSON_VALUE_NULL(name, p, def)                              \
    if (json_reader_read_member(reader, name))                          \
    {                                                                   \
        if (json_reader_get_null_value(reader))                         \
        {                                                               \
            p = _Generic(p,                                             \
                gchar*: g_strdup(def),                                  \
                default: def);                                          \
        }                                                               \
        else                                                            \
        {                                                               \
            p = _Generic(p,                                             \
                gchar*: g_strdup(json_reader_get_string_value(reader)), \
                gint64: json_reader_get_int_value(reader),              \
                gdouble: json_reader_get_double_value(reader),          \
                gboolean: json_reader_get_boolean_value(reader),        \
                GDateTime*: parse_time(json_reader_get_string_value(reader))); \
        }                                                               \
    }                                                                   \
    else                                                                \
    {                                                                   \
        g_set_error(error, GT_TWITCH_ERROR, GT_TWITCH_ERROR_JSON,       \
            "Unable to read JSON member with name '%s'", name);         \
                                                                        \
        WARNINGF("Unable to read JSON member with name '%s'", name);    \
                                                                        \
        goto error;                                                     \
    }                                                                   \
    json_reader_end_member(reader);                                     \

#define CHECK_AND_PROPAGATE_ERROR(msg, ...)                             \
    if (err)                                                            \
    {                                                                   \
        WARNINGF(msg " because: %s", ##__VA_ARGS__, err->message);      \
                                                                        \
        g_propagate_prefixed_error(error, err, msg " because: ", ##__VA_ARGS__); \
                                                                        \
        goto error;                                                     \
    }                                                                   \

typedef struct
{
    SoupSession* soup;

    GThreadPool* image_download_pool;

    GHashTable* emote_table;
    GHashTable* badge_table;
} GtTwitchPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtTwitch, gt_twitch,  G_TYPE_OBJECT)

static GtResourceDownloader* emote_downloader;
static GtResourceDownloader* badge_downloader;

static GtTwitchStreamAccessToken*
gt_twitch_stream_access_token_new()
{
    return g_new0(GtTwitchStreamAccessToken, 1);
}

void
gt_twitch_stream_access_token_free(GtTwitchStreamAccessToken* token)
{
    g_free(token->token);
    g_free(token->sig);
    g_free(token);
}

GtChatEmote*
gt_chat_emote_new()
{
    GtChatEmote* emote = g_new0(GtChatEmote, 1);

    emote->start = -1;
    emote->end = -1;

    return emote;
}

void
gt_chat_emote_free(GtChatEmote* emote)
{
    g_assert_nonnull(emote);
    g_assert(GDK_IS_PIXBUF(emote->pixbuf));

    g_object_unref(emote->pixbuf);
    g_free(emote->code);
    g_free(emote);
}

void
gt_chat_emote_list_free(GList* list)
{
    g_list_free_full(list, (GDestroyNotify) gt_chat_emote_free);
}

GQuark
gt_spawn_twitch_error_quark()
{
    return g_quark_from_static_string("gt-twitch-error-quark");
}

GtTwitch*
gt_twitch_new(void)
{
    return g_object_new(GT_TYPE_TWITCH,
                        NULL);
}

static void
gt_twitch_class_init(GtTwitchClass* klass)
{
}

static void
gt_twitch_init(GtTwitch* self)
{
    GtTwitchPrivate* priv = gt_twitch_get_instance_private(self);

    priv->soup = soup_session_new();
    priv->emote_table = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) g_object_unref);
    priv->badge_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) gt_chat_badge_free);

    g_autofree gchar* emotes_filepath = g_build_filename(g_get_user_cache_dir(),
        "gnome-twitch", "emotes", NULL);

    g_autofree gchar* badges_filepath = g_build_filename(g_get_user_cache_dir(),
        "gnome-twitch", "badges", NULL);

    emote_downloader = gt_resource_downloader_new_with_cache(emotes_filepath);
    gt_resource_downloader_set_image_filetype(emote_downloader, GT_IMAGE_FILETYPE_PNG);

    badge_downloader = gt_resource_downloader_new_with_cache(badges_filepath);
    gt_resource_downloader_set_image_filetype(badge_downloader, GT_IMAGE_FILETYPE_PNG);

    g_signal_connect_swapped(main_app, "shutdown", G_CALLBACK(g_object_unref), emote_downloader);
    g_signal_connect_swapped(main_app, "shutdown", G_CALLBACK(g_object_unref), badge_downloader);
}

static gboolean
send_message(GtTwitch* self, SoupMessage* msg)
{
    GtTwitchPrivate* priv = gt_twitch_get_instance_private(self);
    gboolean ret;
    char* uri = soup_uri_to_string(soup_message_get_uri(msg), FALSE);

    DEBUGF("Sending message to uri '%s'", uri);

    soup_message_headers_append(msg->request_headers, "Client-ID", CLIENT_ID);

    soup_session_send_message(priv->soup, msg);

    ret = SOUP_STATUS_IS_SUCCESSFUL(msg->status_code);

    if (ret)
        TRACEF("Received response from url '%s' with code '%d' and body '%s'",
            uri, msg->status_code, msg->response_body->data);
    else
        WARNINGF("Received unsuccessful response from url '%s' with code '%d' and body '%s'",
            uri, msg->status_code, msg->response_body->data);

    g_free(uri);

    return ret;
}

//TODO: Refactor GtTwitch to use these new functions
static void
new_send_message(GtTwitch* self, SoupMessage* msg, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert(SOUP_IS_MESSAGE(msg));

    GtTwitchPrivate* priv = gt_twitch_get_instance_private(self);
    char* uri = soup_uri_to_string(soup_message_get_uri(msg), FALSE);

    DEBUGF("Sending message to uri '%s'", uri);

    soup_message_headers_append(msg->request_headers, "Client-ID", CLIENT_ID);

    soup_session_send_message(priv->soup, msg);

    if (SOUP_STATUS_IS_SUCCESSFUL(msg->status_code))
    {
        TRACEF("Received response from url '%s' with code '%d' and body '%s'",
               uri, msg->status_code, msg->response_body->data);
    }
    else
    {
        gint code;

        WARNINGF("Received unsuccessful response from url '%s' with code '%d' and body '%s'",
                 uri, msg->status_code, msg->response_body->data);

        code = msg->status_code == GT_TWITCH_ERROR_SOUP_NOT_FOUND ?
            GT_TWITCH_ERROR_SOUP_NOT_FOUND : GT_TWITCH_ERROR_SOUP_GENERIC;

        g_set_error(error, GT_TWITCH_ERROR, code,
            "Received unsuccessful response from url '%s' with code '%d' and body '%s'",
            uri, msg->status_code, msg->response_body->data);
    }

    g_free(uri);
}

static JsonReader*
new_send_message_json_with_version(GtTwitch* self, SoupMessage* msg, const gchar* version, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert(SOUP_IS_MESSAGE(msg));

    JsonReader* ret = NULL;
    g_autofree gchar* accept_header = g_strdup_printf("application/vnd.twitchtv.v%s+json", version);

    soup_message_headers_append(msg->request_headers, "Accept", accept_header);

    new_send_message(self, msg, error);

    if (!*error)
    {
        JsonParser* parser = json_parser_new();
        JsonNode* node = NULL;
        GError* e = NULL;

        json_parser_load_from_data(parser, msg->response_body->data, -1, &e);

        if (e)
        {
            g_set_error(error, GT_TWITCH_ERROR, GT_TWITCH_ERROR_JSON,
                "Error parsing JSON response because: %s", e->message);

            WARNINGF("Error parsing JSON response because: %s", e->message);

            g_error_free(e);

            goto finish;
        }

        node = json_parser_get_root(parser);
        ret = json_reader_new(json_node_ref(node)); //NOTE: Parser doesn't seem to have its own reference to node

        g_object_unref(G_OBJECT(parser));
    }

finish:
    return ret;
}

static JsonReader*
new_send_message_json(GtTwitch* self, SoupMessage* msg, GError** error)
{
    return new_send_message_json_with_version(self, msg, TWITCH_API_VERSION_5, error);
}

static GDateTime*
parse_time(const gchar* time)
{
    GDateTime* ret = NULL;

    gint year, month, day, hour, min, sec;

    sscanf(time, "%d-%d-%dT%d:%d:%dZ", &year, &month, &day, &hour, &min, &sec);

    ret = g_date_time_new_utc(year, month, day, hour, min, sec);

    return ret;
}

static GList*
parse_playlist(const gchar* playlist)
{
    GList* ret = NULL;
    g_auto(GStrv) lines = g_strsplit(playlist, "\n", 0);

    for (gchar** l = lines; *l != NULL; l++)
    {
        if (strncmp(*l, STREAM_INFO, strlen(STREAM_INFO)) == 0)
        {
            GtTwitchStreamData* stream = g_malloc0(sizeof(GtTwitchStreamData));

            g_auto(GStrv) values = g_strsplit(*l, ",", 0);

            for (gchar** m = values; *m != NULL; m++)
            {
                g_auto(GStrv) split = g_strsplit(*m, "=", 0);

                if (strcmp(*split, "BANDWIDTH") == 0)
                {
                    stream->bandwidth = atol(*(split+1));
                }
                else if (strcmp(*split, "RESOLUTION") == 0)
                {
                    gchar** res = g_strsplit(*(split+1), "x", 0);

                    stream->width = atoi(*res);
                    stream->height = atoi(*(res+1));

                    g_strfreev(res);
                }
                else if (strcmp(*split, "VIDEO") == 0)
                {
                    //TODO: Replace these with an error
                    g_assert(g_str_has_prefix(*(split+1), "\""));
                    g_assert(g_str_has_suffix(*(split+1), "\""));

                    //NOTE: quality isn't const but still shouldn't be freed,
                    //it will be freed when split is freed.
                    gchar* quality = *(split+1)+1;
                    quality[strlen(quality) - 1] = '\0';

                    if (STRING_EQUALS(quality, "chunked"))
                        stream->quality = g_strdup("source");
                    else if (STRING_EQUALS(quality, "audio_only")) //NOTE: Remove audio only streams
                    {
                        gt_twitch_stream_data_free(stream);

                        stream = NULL;

                        goto next;
                    }
                    else
                        stream->quality = g_strdup(quality);
                }
            }

            l++;

            stream->url = g_strdup(*l);

        next:
            if (stream) ret = g_list_append(ret, stream);
        }
    }

    return ret;
}

static GtChannelData*
parse_channel(JsonReader* reader, GError** error)
{
    GtChannelData* data = gt_channel_data_new();

    //NOTE: Need this hack until Twitch gets their shit together
    //and standardizes on either integers or strings as IDs
    READ_JSON_MEMBER("_id");
    JsonNode* node = json_reader_get_value(reader);
    if (STRING_EQUALS(json_node_type_name(node), "Integer"))
        data->id = g_strdup_printf("%" G_GINT64_FORMAT, json_reader_get_int_value(reader));
    else if (STRING_EQUALS(json_node_type_name(node), "String"))
        data->id = g_strdup(json_reader_get_string_value(reader));
    else
        g_assert_not_reached();
    END_JSON_MEMBER();

    READ_JSON_VALUE("name", data->name);
    READ_JSON_VALUE_NULL("display_name", data->display_name, NULL);
    READ_JSON_VALUE_NULL("status", data->status, _("Untitled broadcast"));
    READ_JSON_VALUE_NULL("video_banner", data->video_banner_url, NULL);
    READ_JSON_VALUE_NULL("logo", data->logo_url, NULL);
    READ_JSON_VALUE("url", data->profile_url);

    data->online = FALSE;

    return data;

error:
    gt_channel_data_free(data);

    return NULL;
}

static GtChannelData*
parse_stream(JsonReader* reader, GError** error)
{
    GtChannelData* data = NULL;

    READ_JSON_MEMBER("channel");
    data = parse_channel(reader, error);
    END_JSON_MEMBER();

    if (*error) goto error;

    READ_JSON_VALUE_NULL("game", data->game, NULL);
    READ_JSON_VALUE("viewers", data->viewers);
    READ_JSON_VALUE("created_at", data->stream_started_time);
    READ_JSON_MEMBER("preview");
    READ_JSON_VALUE("large", data->preview_url);
    END_JSON_MEMBER();

    data->online = TRUE;

    return data;

error:
    gt_channel_data_free(data);

    return NULL;
}

static GtGameData*
parse_game(JsonReader* reader, GError** error)
{
    GtGameData* data = gt_game_data_new();

    //NOTE: Same hack as above for channel
    READ_JSON_MEMBER("_id");
    JsonNode* node = json_reader_get_value(reader);
    if (STRING_EQUALS(json_node_type_name(node), "Integer"))
        data->id = g_strdup_printf("%" G_GINT64_FORMAT, json_reader_get_int_value(reader));
    else if (STRING_EQUALS(json_node_type_name(node), "String"))
        data->id = g_strdup(json_reader_get_string_value(reader));
    else
        g_assert_not_reached();
    END_JSON_MEMBER();

    READ_JSON_VALUE("name", data->name);
    READ_JSON_MEMBER("box");
    READ_JSON_VALUE("large", data->preview_url);
    END_JSON_MEMBER();
    READ_JSON_MEMBER("logo");
    READ_JSON_VALUE("large", data->logo_url);
    END_JSON_MEMBER();

    return data;

error:
    gt_game_data_free(data);

    return NULL;
}

GtTwitchStreamAccessToken*
gt_twitch_stream_access_token(GtTwitch* self, const gchar* channel, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_false(utils_str_empty(channel));

    g_autoptr(SoupMessage) msg = NULL;
    g_autoptr(JsonReader) reader = NULL;
    g_autofree gchar* uri = NULL;
    GtTwitchStreamAccessToken* ret = NULL;
    GError* err = NULL;

    uri = g_strdup_printf(ACCESS_TOKEN_URI, channel);

    msg = soup_message_new("GET", uri);

    reader = new_send_message_json(self, msg, &err);

    CHECK_AND_PROPAGATE_ERROR("Error getting stream access token for channel '%s'",
        channel);

    ret = gt_twitch_stream_access_token_new();

    READ_JSON_VALUE("sig", ret->sig);
    READ_JSON_VALUE("token", ret->token);

    return ret;

error:
    gt_twitch_stream_access_token_free(ret);

    return NULL;
}

GList*
gt_twitch_all_streams(GtTwitch* self, const gchar* channel, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_false(utils_str_empty(channel));

    g_autoptr(SoupMessage) msg = NULL;
    g_autofree gchar* uri = NULL;
    GtTwitchStreamAccessToken* token = NULL;
    GList* ret = NULL;
    GError* err = NULL;

    token = gt_twitch_stream_access_token(self, channel, &err);

    CHECK_AND_PROPAGATE_ERROR("Unable to get streams for channel '%s'",
        channel);

    uri = g_strdup_printf(STREAM_PLAYLIST_URI, channel,
        token->token, token->sig, g_random_int_range(0, 999999));

    gt_twitch_stream_access_token_free(token);

    msg = soup_message_new("GET", uri);

    new_send_message(self, msg, &err);

    CHECK_AND_PROPAGATE_ERROR("Unable to get all streams for channel '%s'",
        channel);

    ret = parse_playlist(msg->response_body->data);

error:
    return ret;
}

static void
all_streams_cb(GTask* task, gpointer source,
    gpointer task_data, GCancellable* cancel)
{
    g_assert(G_IS_TASK(task));
    g_assert(GT_IS_TWITCH(source));
    g_assert_nonnull(task_data);

    GenericTaskData* data = NULL;
    GList* ret = NULL;
    GError* err = NULL;

    if (g_task_return_error_if_cancelled(task))
        return;

    data = task_data;

    ret = gt_twitch_all_streams(GT_TWITCH(source), data->str_1, &err);

    if (err)
        g_task_return_error(task, err);
    else
        g_task_return_pointer(task, ret, (GDestroyNotify) gt_twitch_stream_data_list_free);
}

void
gt_twitch_all_streams_async(GtTwitch* self, const gchar* channel,
    GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata)
{
    GTask* task;
    GenericTaskData* data;

    task = g_task_new(self, cancel, cb, udata);

    g_task_set_return_on_cancel(task, FALSE);

    data = generic_task_data_new();
    data->str_1 = g_strdup(channel);

    g_task_set_task_data(task, data, (GDestroyNotify) generic_task_data_free);

    g_task_run_in_thread(task, all_streams_cb);
}

GList*
gt_twitch_all_streams_finish(GtTwitch* self, GAsyncResult* result, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert(G_IS_ASYNC_RESULT(result));

    GList* ret = g_task_propagate_pointer(G_TASK(result), error);

    return ret;
}

const GtTwitchStreamData*
gt_twitch_stream_list_filter_quality(GList* list,
    const gchar* quality)
{
    GtTwitchStreamData* ret = NULL;

    for (GList* l = list; l != NULL; l = l->next)
    {
        ret = (GtTwitchStreamData*) l->data;

        if (STRING_EQUALS(ret->quality, quality))
            break;
        else
            ret = NULL;
    }

    if (!ret)
    {
        ret = list->data;
        g_assert_nonnull(ret);
    }

    return ret;
}

GList*
gt_twitch_top_channels(GtTwitch* self, gint n, gint offset,
    const gchar* game, const gchar* language, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_cmpint(n, >=, 0);
    g_assert_cmpint(n, <=, 100);
    g_assert_cmpint(offset, >=, 0);
    g_assert_nonnull(game);
    g_assert_nonnull(language);

    g_autoptr(SoupMessage) msg = NULL;
    g_autoptr(JsonReader) reader = NULL;
    g_autofree gchar* uri = NULL;
    GList* ret = NULL;
    GError* err = NULL;

    uri = g_strdup_printf(TOP_CHANNELS_URI, n, offset, game, language);

    msg = soup_message_new("GET", uri);

    reader = new_send_message_json(self, msg, &err);

    CHECK_AND_PROPAGATE_ERROR("Unable to fetch top channels with amount '%d', offset '%d' and game '%s'",
        n, offset, game);

    READ_JSON_MEMBER("streams");

    for (gint i = 0; i < json_reader_count_elements(reader); i++)
    {
        GtChannel* channel = NULL;
        GtChannelData* data = NULL;

        READ_JSON_ELEMENT(i);

        data = parse_stream(reader, &err);

        CHECK_AND_PROPAGATE_ERROR("Unable to fetch top channels with amount '%d', offset '%d' and game '%s'",
            n, offset, game);

        channel = gt_channel_new(data);

        END_JSON_ELEMENT();

        ret = g_list_append(ret, channel);
    }

    END_JSON_MEMBER();

    return ret;

error:
    gt_channel_list_free(ret);

    return NULL;
}
static void
top_channels_async_cb(GTask* task,
                      gpointer source,
                      gpointer task_data,
                      GCancellable* cancel)
{
    g_assert(GT_IS_TWITCH(source));
    g_assert(G_IS_TASK(task));
    g_assert_nonnull(task_data);

    GenericTaskData* data = task_data;
    GList* ret = NULL;
    GError* err = NULL;

    if (g_task_return_error_if_cancelled(task))
        return;

    ret = gt_twitch_top_channels(GT_TWITCH(source), data->int_1, data->int_2,
        data->str_1, data->str_2, &err);

    if (err)
        g_task_return_error(task, err);
    else
        g_task_return_pointer(task, ret, (GDestroyNotify) gt_channel_list_free);
}

void
gt_twitch_top_channels_async(GtTwitch* self, gint n, gint offset, const gchar* game,
    const gchar* language, GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_cmpint(n, >=, 0);
    g_assert_cmpint(n, <=, 100);
    g_assert_cmpint(offset, >=, 0);
    g_assert_nonnull(game);
    g_assert_nonnull(language);

    GTask* task = NULL;
    GenericTaskData* data = NULL;

    task = g_task_new(self, cancel, cb, udata);
    g_task_set_return_on_cancel(task, FALSE);

    data = generic_task_data_new();
    data->int_1 = n;
    data->int_2 = offset;
    data->str_1 = g_strdup(game);
    data->str_2 = g_strdup(language);

    g_task_set_task_data(task, data, (GDestroyNotify) generic_task_data_free);

    g_task_run_in_thread(task, top_channels_async_cb);

    g_object_unref(task);
}

GList*
gt_twitch_top_channels_finish(GtTwitch* self,
    GAsyncResult* result, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert(G_IS_ASYNC_RESULT(result));

    GList* ret = g_task_propagate_pointer(G_TASK(result), error);

    return ret;
}

GList*
gt_twitch_top_games(GtTwitch* self,
    gint n, gint offset, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_cmpint(n, >=, 0);
    g_assert_cmpint(n, <=, 100);
    g_assert_cmpint(offset, >=, 0);

    g_autoptr(SoupMessage) msg = NULL;
    g_autoptr(JsonReader) reader = NULL;
    g_autofree gchar* uri = NULL;
    GList* ret = NULL;
    GError* err = NULL;

    uri = g_strdup_printf(TOP_GAMES_URI, n, offset);

    msg = soup_message_new("GET", uri);

    reader = new_send_message_json(self, msg, &err);

    CHECK_AND_PROPAGATE_ERROR("Unable to get top games with amount '%d' and offset '%d'",
        n, offset);

    READ_JSON_MEMBER("top");

    for (gint i = 0; i < json_reader_count_elements(reader); i++)
    {
        GtGame* game = NULL;
        GtGameData* data = NULL;

        READ_JSON_ELEMENT(i);

        READ_JSON_MEMBER("game");

        data = parse_game(reader, &err);

        CHECK_AND_PROPAGATE_ERROR("Unable to get top games with amount '%d' and offset '%d'",
            n, offset);

        END_JSON_MEMBER();

        READ_JSON_VALUE("viewers", data->viewers);
        READ_JSON_VALUE("channels", data->channels);

        game = gt_game_new(data);

        END_JSON_ELEMENT();

        ret = g_list_append(ret, game);
    }

    END_JSON_MEMBER();

    return ret;

error:
    gt_game_list_free(ret);

    return NULL;
}


static void
top_games_async_cb(GTask* task,
                   gpointer source,
                   gpointer task_data,
                   GCancellable* cancel)
{
    g_assert(GT_IS_TWITCH(source));
    g_assert(G_IS_TASK(task));
    g_assert_nonnull(task_data);

    GenericTaskData* data = task_data;
    GList* ret = NULL;
    GError* err = NULL;

    if (g_task_return_error_if_cancelled(task))
        return;

    ret = gt_twitch_top_games(GT_TWITCH(source), data->int_1, data->int_2, &err);

    if (err)
        g_task_return_error(task, err);
    else
        g_task_return_pointer(task, ret, (GDestroyNotify) gt_game_list_free);
}

void
gt_twitch_top_games_async(GtTwitch* self, gint n, gint offset,
                             GCancellable* cancel,
                             GAsyncReadyCallback cb,
                             gpointer udata)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_cmpint(n, >=, 0);
    g_assert_cmpint(n, <=, 100);
    g_assert_cmpint(offset, >=, 0);

    GTask* task = NULL;
    GenericTaskData* data = NULL;

    task = g_task_new(self, cancel, cb, udata);
    g_task_set_return_on_cancel(task, FALSE);

    data = generic_task_data_new();
    data->int_1 = n;
    data->int_2 = offset;

    g_task_set_task_data(task, data, (GDestroyNotify) generic_task_data_free);

    g_task_run_in_thread(task, top_games_async_cb);

    g_object_unref(task);
}

GList*
gt_twitch_top_games_finish(GtTwitch* self,
    GAsyncResult* result, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert(G_IS_ASYNC_RESULT(result));

    GList* ret = g_task_propagate_pointer(G_TASK(result), error);

    return ret;
}

//NOTE: Twitch's stream search API is retarted (see https://github.com/justintv/Twitch-API/issues/513)
//so we need to do this hack to get anything remotely usable. It will return duplicates unless
//amount=offset*k where k is some multiple, i.e. it works in 'pages'
GList*
gt_twitch_search_channels(GtTwitch* self, const gchar* query, gint n, gint offset, gboolean offline, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_cmpint(n, >=, 0);
    g_assert_cmpint(n, <=, 100);
    g_assert_cmpint(offset, >=, 0);
    g_assert_false(utils_str_empty(query));

#define SEARCH_AMOUNT 100

    const gint PAGE_AMOUNT = offline ? 100 : 90;

    MESSAGEF("Searching for channels with query '%s', amount '%d' ('%d') and offset '%d' ('%d')",
        query, 100, SEARCH_AMOUNT, (offset / PAGE_AMOUNT) * 100, offset);

    g_autoptr(SoupMessage) msg = NULL;
    g_autoptr(JsonReader) reader = NULL;
    g_autofree gchar* uri = NULL;
    gint total;
    GList* ret = NULL;
    GError* err = NULL;

    uri = g_strdup_printf(offline ? SEARCH_CHANNELS_URI : SEARCH_STREAMS_URI,
        query, SEARCH_AMOUNT, (offset / PAGE_AMOUNT) * SEARCH_AMOUNT);

    msg = soup_message_new("GET", uri);

    reader = new_send_message_json(self, msg, &err);

    CHECK_AND_PROPAGATE_ERROR("Unable to search channels with query '%s', amount '%d' ('%d') and offset '%d' ('%d')",
        query, PAGE_AMOUNT, SEARCH_AMOUNT, (offset / PAGE_AMOUNT) * PAGE_AMOUNT, offset);

    READ_JSON_MEMBER(offline ? "channels" : "streams");

    total = MIN(n + offset % PAGE_AMOUNT, json_reader_count_elements(reader));

    for (gint i = offset % PAGE_AMOUNT; i < total; i++)
    {
        GtChannel* channel = NULL;
        GtChannelData* data = NULL;

        READ_JSON_ELEMENT(i);

        data = offline ? parse_channel(reader, &err) : parse_stream(reader, &err);

        CHECK_AND_PROPAGATE_ERROR("Unable to search channels with query '%s', amount '%d' ('%d') and offset '%d' ('%d')",
            query, PAGE_AMOUNT, SEARCH_AMOUNT, (offset / PAGE_AMOUNT) * PAGE_AMOUNT, offset);

        channel = gt_channel_new(data);

        END_JSON_ELEMENT();

        ret = g_list_append(ret, channel);
    }

    json_reader_end_member(reader);

    return ret;

error:
    gt_channel_list_free(ret);

    return NULL;
}

static void
search_channels_async_cb(GTask* task, gpointer source,
    gpointer task_data, GCancellable* cancel)
{
    g_assert(GT_IS_TWITCH(source));
    g_assert(G_IS_TASK(task));
    g_assert_nonnull(task_data);

    GenericTaskData* data = task_data;

    if (g_task_return_error_if_cancelled(task))
        return;

    GError* err = NULL;
    GList* ret = gt_twitch_search_channels(GT_TWITCH(source), data->str_1,
        data->int_1, data->int_2, data->bool_1, &err);

    if (err)
        g_task_return_error(task, err);
    else
        g_task_return_pointer(task, ret, (GDestroyNotify) gt_channel_list_free);
}

void
gt_twitch_search_channels_async(GtTwitch* self,
    const gchar* query, gint n, gint offset, gboolean offline,
    GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_cmpint(n, >=, 0);
    g_assert_cmpint(n, <=, 100);
    g_assert_cmpint(offset, >=, 0);
    g_assert_false(utils_str_empty(query));

    GTask* task = NULL;
    GenericTaskData* data = NULL;

    task = g_task_new(self, cancel, cb, udata);
    g_task_set_return_on_cancel(task, FALSE);

    data = generic_task_data_new();
    data->int_1 = n;
    data->int_2 = offset;
    data->str_1 = g_strdup(query);
    data->bool_1 = offline;

    g_task_set_task_data(task, data, (GDestroyNotify) generic_task_data_free);

    g_task_run_in_thread(task, search_channels_async_cb);

    g_object_unref(task);
}

GList*
gt_twitch_search_channels_finish(GtTwitch* self,
    GAsyncResult* result, GError** error)

{
    g_assert(GT_IS_TWITCH(self));
    g_assert(G_IS_ASYNC_RESULT(result));

    GList* ret = g_task_propagate_pointer(G_TASK(result), error);

    return ret;
}

GList*
gt_twitch_search_games(GtTwitch* self,
    const gchar* query, gint n, gint offset,
    GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_cmpint(n, >=, 0);
    g_assert_cmpint(n, <=, 100);
    g_assert_cmpint(offset, >=, 0);
    g_assert_false(utils_str_empty(query));

    g_autoptr(SoupMessage) msg = NULL;
    g_autoptr(JsonReader) reader = NULL;
    g_autofree gchar* uri = NULL;
    GList* ret = NULL;
    GError* err = NULL;

    uri = g_strdup_printf(SEARCH_GAMES_URI, query);

    msg = soup_message_new("GET", uri);

    reader = new_send_message_json(self, msg, &err);

    CHECK_AND_PROPAGATE_ERROR("Unable to search games with query '%s', amount '%d' and offset '%d'",
        query, n, offset);

    READ_JSON_MEMBER("games");

    for (gint i = 0; i < json_reader_count_elements(reader); i++)
    {
        GtGame* game;
        GtGameData* data = NULL;

        READ_JSON_ELEMENT(i);

        data = parse_game(reader, &err);

        CHECK_AND_PROPAGATE_ERROR("Unable to search games with query '%s', amount '%d' and offset '%d'",
            query, n, offset);

        game = gt_game_new(data);

        END_JSON_ELEMENT();

        ret = g_list_append(ret, game);
    }

    END_JSON_MEMBER();

    return ret;

error:
    gt_game_list_free(ret);

    return NULL;
}


static void
search_games_async_cb(GTask* task, gpointer source,
    gpointer task_data, GCancellable* cancel)
{
    g_assert(GT_IS_TWITCH(source));
    g_assert(G_IS_TASK(task));
    g_assert_nonnull(task_data);

    GenericTaskData* data = task_data;

    if (g_task_return_error_if_cancelled(task))
        return;

    GError* err = NULL;
    GList* ret = gt_twitch_search_games(GT_TWITCH(source), data->str_1, data->int_1, data->int_2, &err);

    if (err)
        g_task_return_error(task, err);
    else
        g_task_return_pointer(task, ret, (GDestroyNotify) gt_game_list_free);
}

void
gt_twitch_search_games_async(GtTwitch* self,
    const gchar* query, gint n, gint offset,
    GCancellable* cancel, GAsyncReadyCallback cb,
    gpointer udata)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_cmpint(n, >=, 0);
    g_assert_cmpint(n, <=, 100);
    g_assert_cmpint(offset, >=, 0);
    g_assert_false(utils_str_empty(query));

    GTask* task = NULL;
    GenericTaskData* data = NULL;

    task = g_task_new(self, cancel, cb, udata);
    g_task_set_return_on_cancel(task, FALSE);

    data = generic_task_data_new();
    data->int_1 = n;
    data->int_2 = offset;
    data->str_1 = g_strdup(query);

    g_task_set_task_data(task, data, (GDestroyNotify) generic_task_data_free);

    g_task_run_in_thread(task, search_games_async_cb);

    g_object_unref(task);
}

GList*
gt_twitch_search_games_finish(GtTwitch* self,
    GAsyncResult* result, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert(G_IS_ASYNC_RESULT(result));

    GList* ret = g_task_propagate_pointer(G_TASK(result), error);

    return ret;
}

void
gt_twitch_stream_data_free(GtTwitchStreamData* data)
{
    g_free(data->quality);
    g_free(data->url);
    g_free(data);
}

void
gt_twitch_stream_data_list_free(GList* list)
{
    g_list_free_full(list, (GDestroyNotify) gt_twitch_stream_data_free);
}

GtChannelData*
gt_twitch_fetch_channel_data(GtTwitch* self, const gchar* id, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_false(utils_str_empty(id));

    g_autoptr(SoupMessage) msg = NULL;
    g_autoptr(JsonReader) reader = NULL;
    g_autofree gchar* uri = NULL;
    GError* err = NULL;
    GtChannelData* ret = NULL;

    uri = g_strdup_printf(FETCH_STREAM_URI, id);

    msg = soup_message_new("GET", uri);

    reader = new_send_message_json(self, msg, &err);

    CHECK_AND_PROPAGATE_ERROR("Unable to fetch channel data with id '%s'",
        id);

    ret = gt_channel_data_new();

    READ_JSON_MEMBER("stream");

    if (json_reader_get_null_value(reader))
    {
        //NOTE: Free these here as they will be used again
        g_object_unref(msg);
        g_object_unref(reader);
        g_free(uri);

        uri = g_strdup_printf(FETCH_CHANNEL_URI, id);

        msg = soup_message_new("GET", uri);

        reader = new_send_message_json(self, msg, &err);

        CHECK_AND_PROPAGATE_ERROR("Unable to fetch channel data with id '%s'",
            id);

        ret = parse_channel(reader, &err);

        CHECK_AND_PROPAGATE_ERROR("Unable to fetch channel data with id '%s'",
            id);
    }
    else
    {
        ret = parse_stream(reader, &err);

        CHECK_AND_PROPAGATE_ERROR("Unable to fetch channel data with id '%s'",
            id);

        END_JSON_MEMBER();
    }

    return ret;

error:

    gt_channel_data_free(ret);

    return NULL;
}

GtChannel*
gt_twitch_fetch_channel(GtTwitch* self, const gchar* id, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_false(utils_str_empty(id));

    GtChannelData* data = NULL;
    GtChannel* channel = NULL;

    DEBUG("Fetching channel with id '%s'", id);

    data = gt_twitch_fetch_channel_data(self, id, error);

    if (*error)
        return NULL;

    channel = gt_channel_new(data);

    g_assert(g_object_is_floating(channel));

    g_object_ref_sink(channel);

    return channel;
}

static void
fetch_channel_async_cb(GTask* task, gpointer source,
    gpointer task_data, GCancellable* cancel)
{
    g_assert(GT_IS_TWITCH(source));
    g_assert(G_IS_TASK(task));
    g_assert_nonnull(task_data);

    GError* err = NULL;
    GenericTaskData* data = task_data;

    GtChannel* ret = gt_twitch_fetch_channel(GT_TWITCH(source), data->str_1, &err);

    if (err)
        g_task_return_error(task, err);
    else
        g_task_return_pointer(task, ret, (GDestroyNotify) g_object_unref);
}

void
gt_twitch_fetch_channel_async(GtTwitch* self,
    const gchar* id, GAsyncReadyCallback cb,
    GCancellable* cancel, gpointer udata)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_false(utils_str_empty(id));

    GTask* task = NULL;
    GenericTaskData* data = generic_task_data_new();

    task = g_task_new(self, cancel, cb, udata);

    data->str_1 = g_strdup(id);

    g_task_set_task_data(task, data, (GDestroyNotify) generic_task_data_free);

    g_task_run_in_thread(task, fetch_channel_async_cb);

    g_object_unref(task);
}

GtChannel*
gt_twitch_fetch_channel_finish(GtTwitch* self,
    GAsyncResult* result, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert(G_IS_ASYNC_RESULT(result));

    GtChannel* ret = g_task_propagate_pointer(G_TASK(result), error);

    return ret;
}

/* GdkPixbuf* */
/* gt_twitch_download_picture_new(GtTwitch* self, const gchar* uri, */
/*     gboolean check_cache, GError** error) */
/* { */
/*     g_return_val_if_fail(GT_IS_TWITCH(self), NULL); */
/*     g_return_val_if_fail(!utils_str_empty(uri), NULL); */

/*     GtTwitchPrivate* priv = gt_twitch_get_instance_private(self); */
/*     GdkPixbuf* ret = NULL; */

/*     g_autoptr(SoupMessage) msg = NULL; */

/*     if (check_cache) */
/*     { */
/*         g_autofree gchar* filename= NULL; */
/*         gchar hash_str[15]; */
/*         guint hash = 0; */
/*         gint64 timestamp = 0; */

/*         hash = g_str_hash(uri); */

/*         g_sprintf(hash_str, "%ud", hash); */

/*         filename = g_build_filename(g_get_user_cache_dir(), */
/*             "gnome-twitch", "images", NULL); */

/*         if (g_file_test(filename, G_FILE_TEST_EXISTS)) */
/*         { */
/*             ret = */
/*         } */
/*     } */
/* } */

GdkPixbuf*
gt_twitch_download_picture(GtTwitch* self, const gchar* url, gint64 timestamp, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_false(utils_str_empty(url));

    GtTwitchPrivate* priv = gt_twitch_get_instance_private(self);
    g_autoptr(SoupMessage) msg = NULL;
    GError* err = NULL;
    GdkPixbuf* ret = NULL;
    g_autoptr(GInputStream) input_stream = NULL;

    DEBUG("Downloading picture from url '%s'", url);

    if (timestamp)
    {
        msg = soup_message_new(SOUP_METHOD_HEAD, url);
        soup_message_headers_append(msg->request_headers, "Client-ID", CLIENT_ID);
        soup_session_send_message(priv->soup, msg);

        if (SOUP_STATUS_IS_SUCCESSFUL(msg->status_code))
        {
            const gchar* last_modified;

            if ((last_modified = soup_message_headers_get_one(msg->response_headers, "Last-Modified")) != NULL &&
                utils_http_full_date_to_timestamp(last_modified) < timestamp)
            {
                DEBUG("No new content at url '%s'", url);

                return NULL;
            }
        }
        else
        {
            WARNING("Unable to download picture from url '%s' because received unsuccessful response with code '%d' and body '%s'",
                url, msg->status_code, msg->response_body->data);

            g_set_error(error, GT_TWITCH_ERROR, GT_TWITCH_ERROR_SOUP_GENERIC,
                "Unable to download picture from url '%s' because received unsuccessful response with code '%d' and body '%s'",
                url, msg->status_code, msg->response_body->data);

            return NULL;
        }
    }

#define CHECK_ERROR                                                     \
    if (err)                                                            \
    {                                                                   \
        WARNINGF("Error downloading picture for url '%s' because: %s",  \
            url, err->message);                                         \
                                                                        \
        g_propagate_prefixed_error(error, err, "Unable to download picture for url '%s' because: ", \
            url);                                                       \
                                                                        \
        return NULL;                                                    \
    }                                                                   \


    msg = soup_message_new(SOUP_METHOD_GET, url);
    soup_message_headers_append(msg->request_headers, "Client-ID", CLIENT_ID);
    input_stream = soup_session_send(priv->soup, msg, NULL, &err);

    CHECK_ERROR;

    ret = gdk_pixbuf_new_from_stream(input_stream, NULL, &err);

    CHECK_ERROR;

    //TODO: Need to free ret if an error is encountered here
    g_input_stream_close(input_stream, NULL, &err);

    CHECK_ERROR;

    return ret;
}

static void
download_picture_async_cb(GTask* task,
                          gpointer source,
                          gpointer task_data,
                          GCancellable* cancel)
{
    GenericTaskData* data = (GenericTaskData*) task_data;
    GdkPixbuf* ret = NULL;
    GError* err = NULL;

    if (g_task_return_error_if_cancelled(task))
        return;

    ret = gt_twitch_download_picture(GT_TWITCH(source), data->str_1, data->int_1, &err);

    if (err)
        g_task_return_error(task, err);
    else
        g_task_return_pointer(task, ret, (GDestroyNotify) g_object_unref);
}

void
gt_twitch_download_picture_async(GtTwitch* self,
    const gchar* url, gint64 timestamp,
    GCancellable* cancel, GAsyncReadyCallback cb,
    gpointer udata)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_false(utils_str_empty(url));

    GTask* task = NULL;
    GenericTaskData* data = NULL;

    task = g_task_new(self, cancel, cb, udata);
    g_task_set_return_on_cancel(task, FALSE);

    data = generic_task_data_new();
    data->str_1 = g_strdup(url);
    data->int_1 = timestamp;

    g_task_set_task_data(task, data, (GDestroyNotify) generic_task_data_free);

    g_task_run_in_thread(task, download_picture_async_cb);

    g_object_unref(task);
}

GdkPixbuf*
gt_twitch_download_emote(GtTwitch* self, gint id)
{
    GtTwitchPrivate* priv = gt_twitch_get_instance_private(self);
    GdkPixbuf* ret = NULL;

    if (!g_hash_table_contains(priv->emote_table, GINT_TO_POINTER(id)))
    {
        g_autofree gchar* uri = NULL;
        g_autoptr(GError) err = NULL;
        g_autoptr(GdkPixbuf) emote = NULL;
        gchar id_str[15];

        uri = g_strdup_printf(TWITCH_EMOTE_URI, id, 1);
        g_sprintf(id_str, "%d", id);

        DEBUGF("Downloading emote form url='%s'", uri);

        emote = gt_resource_downloader_download_image(emote_downloader, uri, id_str, &err);

        /* NOTE: If we encountered an error here we'll just insert a generic error emote */
        if (err)
        {
            g_autoptr(GtkIconInfo) icon_info;

            WARNING("Unable to download emote with id '%d' because: %s", id, err->message);

            g_clear_error(&err);

            icon_info = gtk_icon_theme_lookup_icon(gtk_icon_theme_get_default(),
                "software-update-urgent-symbolic", 1, 0);

            emote = gtk_icon_info_load_icon(icon_info, &err);

            RETURN_VAL_IF_FAIL(err == NULL, NULL);
        }

        g_hash_table_insert(priv->emote_table, GINT_TO_POINTER(id),
            g_steal_pointer(&emote));

        //TODO: Propagate this error further
        RETURN_VAL_IF_FAIL(err == NULL, NULL);
    }

    ret = GDK_PIXBUF(g_hash_table_lookup(priv->emote_table, GINT_TO_POINTER(id)));

    g_object_ref(ret);

    return ret;
}

static void
fetch_chat_badge_set(GtTwitch* self, const gchar* set_name, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_false(utils_str_empty(set_name));

    GtTwitchPrivate* priv = gt_twitch_get_instance_private(self);
    g_autoptr(SoupMessage) msg = NULL;
    g_autoptr(JsonReader) reader = NULL;
    g_autofree gchar* uri = NULL;
    GError* err = NULL;

    g_assert_false(g_hash_table_contains(priv->badge_table, set_name));

    g_hash_table_add(priv->badge_table, g_strdup(set_name)); //NOTE: This will be freed by the hash table when it's destroyed

    INFOF("Fetching chat badge set with name '%s'", set_name);

    uri = g_strcmp0(set_name, "global") == 0 ? g_strdup_printf(GLOBAL_CHAT_BADGES_URI) :
        g_strdup_printf(NEW_CHAT_BADGES_URI, set_name);

    msg = soup_message_new("GET", uri);

    reader = new_send_message_json(self, msg, &err);

    CHECK_AND_PROPAGATE_ERROR("Error fetching chat badges for set %s", set_name);

    READ_JSON_MEMBER("badge_sets");

    for (gint i = 0; i < json_reader_count_members(reader); i++)
    {
        READ_JSON_ELEMENT(i);

        const gchar* badge_name = json_reader_get_member_name(reader);

        READ_JSON_MEMBER("versions");

        for (gint j = 0; j < json_reader_count_members(reader); j++)
        {
            GtChatBadge* badge = gt_chat_badge_new();
            /* NOTE: Don't need to free this as it's freed by the hash table when it's destroyed. */
            gchar* key = NULL;
            g_autofree gchar* uri = NULL;

            READ_JSON_ELEMENT(j);

            badge->name = g_strdup(badge_name);
            badge->version = g_strdup(json_reader_get_member_name(reader));

            key = g_strdup_printf("%s-%s-%s", set_name, badge->name, badge->version);

            READ_JSON_VALUE("image_url_1x", uri);
            badge->pixbuf = gt_resource_downloader_download_image(badge_downloader,
                uri, key, &err);

            if (err)
            {
                g_autoptr(GtkIconInfo) icon_info;

                WARNING("Unable to fetch chat badge set with name '%s' because: %s",
                    set_name, err->message);

                g_clear_error(&err);

                /* NOTE: If we encountered an error here we'll just insert a generic error emote */

                g_clear_error(&err);

                icon_info = gtk_icon_theme_lookup_icon(gtk_icon_theme_get_default(),
                    "software-update-urgent-symbolic", 1, 0);

                badge->pixbuf = gtk_icon_info_load_icon(icon_info, &err);

                RETURN_IF_FAIL(err == NULL);

            }

            END_JSON_ELEMENT();


            g_assert_false(g_hash_table_contains(priv->badge_table, key));

            g_hash_table_insert(priv->badge_table, key, badge);

            DEBUGF("Downloaded emote for set '%s' with name '%s' and version '%s'", set_name,
                badge->name, badge->version);
        }

        END_JSON_MEMBER();
        END_JSON_ELEMENT();
    }

    END_JSON_MEMBER();

error:
    return;
}

void
gt_twitch_load_chat_badge_sets_for_channel(GtTwitch* self, const gchar* chan_id, GError** error)
{
    g_assert(GT_IS_TWITCH(self));

    GtTwitchPrivate* priv = gt_twitch_get_instance_private(self);

#define FETCH_BADGE_SET(s)                                              \
    if (!g_hash_table_contains(priv->badge_table, s))                   \
    {                                                                   \
        GError* err = NULL;                                             \
                                                                        \
        fetch_chat_badge_set(self, s, &err);                            \
                                                                        \
        if (err)                                                        \
        {                                                               \
            WARNINGF("Unable to load chat badge sets for channel '%s'", \
                chan_id);                                               \
                                                                        \
            g_propagate_prefixed_error(error, err,                      \
                "Unable to load chat badge sets for channel '%s' because: ", chan_id); \
                                                                        \
            return;                                                     \
        }                                                               \
    }                                                                   \

    FETCH_BADGE_SET("global");
    FETCH_BADGE_SET(chan_id);

#undef FETCH_BADGE_SET
}

// NOTE: This will automatically download any badge sets if they
// aren't already
GtChatBadge*
gt_twitch_fetch_chat_badge(GtTwitch* self,
    const gchar* chan_id, const gchar* badge_name,
    const gchar* version, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_false(utils_str_empty(badge_name));
    g_assert_false(utils_str_empty(version));

    GtTwitchPrivate* priv = gt_twitch_get_instance_private(self);
    g_autofree gchar* global_key = NULL;
    g_autofree gchar* chan_key = NULL;
    GtChatBadge* ret = NULL;
    GError* err = NULL;

    gt_twitch_load_chat_badge_sets_for_channel(self, chan_id, &err);

    CHECK_AND_PROPAGATE_ERROR("Unable to fetch chat badge for channel '%s with badge name '%s' and version '%s'",
        chan_id, badge_name, version);

    global_key = g_strdup_printf("global-%s-%s", badge_name, version);
    chan_key = g_strdup_printf("%s-%s-%s", chan_id, badge_name, version);

    if (g_hash_table_contains(priv->badge_table, chan_key))
        ret = g_hash_table_lookup(priv->badge_table, chan_key);
    else if (g_hash_table_contains(priv->badge_table, global_key))
        ret = g_hash_table_lookup(priv->badge_table, global_key);
    else
        g_assert_not_reached(); //NOTE: We might as well crash here as the badge being null would lead to many problems

error:
    return ret;
}

void
fetch_chat_badge_async_cb(GTask* task, gpointer source,
    gpointer task_data, GCancellable* cancel)
{
    g_assert(GT_IS_TWITCH(source));
    g_assert(G_IS_TASK(task));
    g_assert_nonnull(task_data);

    GenericTaskData* data;
    GtChatBadge* ret = NULL;
    GError* err = NULL;

    if (g_task_return_error_if_cancelled(task))
        return;

    data = task_data;

    ret = gt_twitch_fetch_chat_badge(GT_TWITCH(source),
        data->str_1, data->str_2, data->str_3, &err);

    if (err)
        g_task_return_error(task, err);
    else
        g_task_return_pointer(task, ret, (GDestroyNotify) gt_chat_badge_list_free);
}

void
gt_twitch_fetch_chat_badge_async(GtTwitch* self,
    const gchar* chan_id, const gchar* badge_name, const gchar* version,
    GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_false(utils_str_empty(badge_name));
    g_assert_false(utils_str_empty(version));

    GTask* task;
    GenericTaskData* data;

    task = g_task_new(self, cancel, cb, udata);
    g_task_set_return_on_cancel(task, FALSE);

    data = generic_task_data_new();
    data->str_1 = g_strdup(chan_id);
    data->str_2 = g_strdup(badge_name);
    data->str_3 = g_strdup(version);

    g_task_set_task_data(task, data, (GDestroyNotify) generic_task_data_free);

    g_task_run_in_thread(task, fetch_chat_badge_async_cb);

    g_object_unref(task);
}

GtChatBadge*
gt_twitch_fetch_chat_badge_finish(GtTwitch* self,
    GAsyncResult* result, GError** err)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert(G_IS_ASYNC_RESULT(result));

    GtChatBadge* ret = NULL;

    ret = g_task_propagate_pointer(G_TASK(result), err);

    return ret;
}

static GtTwitchChannelInfoPanel*
gt_twitch_channel_info_panel_new()
{
    return g_new0(GtTwitchChannelInfoPanel, 1);
}

void
gt_twitch_channel_info_panel_free(GtTwitchChannelInfoPanel* panel)
{
    g_free(panel->html_description);
    g_free(panel->markdown_description);
    g_clear_object(&panel->image);
    g_free(panel->link);
    g_free(panel);
}

void
gt_twitch_channel_info_panel_list_free(GList* list)
{
    g_list_free_full(list, (GDestroyNotify) gt_twitch_channel_info_panel_free);
}

//TODO: Rewrite this once we start displaying channel info again
GList*
gt_twitch_channel_info(GtTwitch* self, const gchar* chan)
{
    GtTwitchPrivate* priv = gt_twitch_get_instance_private(self);
    SoupMessage* msg;
    gchar* uri = NULL;
    JsonParser* parser;
    JsonNode* node;
    JsonReader* reader;
    GList* ret = NULL;

    INFOF("Getting channel info for='%s'", chan);

    uri = g_strdup_printf(CHANNEL_INFO_URI, chan);

    msg = soup_message_new("GET", uri);

    if (!send_message(self, msg))
    {
        WARNINGF("Error getting chat badges for channel='%s'", chan);
        goto finish;
    }

    parser = json_parser_new();
    json_parser_load_from_data(parser, msg->response_body->data, msg->response_body->length, NULL); //TODO: Error handling
    node = json_parser_get_root(parser);
    reader = json_reader_new(node);

    for (gint i = 0; i < json_reader_count_elements(reader); i++)
    {
        GtTwitchChannelInfoPanel* panel = gt_twitch_channel_info_panel_new();
        const gchar* type = NULL;

        json_reader_read_element(reader, i);

        json_reader_read_member(reader, "display_order");
        panel->order = json_reader_get_int_value(reader) - 1;
        json_reader_end_member(reader);

        json_reader_read_member(reader, "kind");
        type = json_reader_get_string_value(reader);
        if (g_strcmp0(type, "default") == 0)
        {
            panel->type = GT_TWITCH_CHANNEL_INFO_PANEL_TYPE_DEFAULT;
        }
        else
        {
            //TODO: Eventually handle other types of panels
            gt_twitch_channel_info_panel_free(panel);
            json_reader_end_member(reader);
            json_reader_end_element(reader);
            continue;
        }
        json_reader_end_member(reader);

        json_reader_read_member(reader, "html_description");
        if (!json_reader_get_null_value(reader))
            panel->html_description = g_strdup(json_reader_get_string_value(reader));
        json_reader_end_member(reader);

        json_reader_read_member(reader, "data");

        json_reader_read_member(reader, "link");
        panel->link = g_strdup(json_reader_get_string_value(reader));
        json_reader_end_member(reader);

        json_reader_read_member(reader, "image");
        /* panel->image = gt_twitch_download_picture(self, json_reader_get_string_value(reader), 0); */
        json_reader_end_member(reader);

        if (json_reader_read_member(reader, "description"))
            panel->markdown_description = g_strdup(json_reader_get_string_value(reader));
        json_reader_end_member(reader);

        if (json_reader_read_member(reader, "title"))
            panel->title = g_strdup(json_reader_get_string_value(reader));
        json_reader_end_member(reader);

        json_reader_end_member(reader);

        json_reader_end_element(reader);

        ret = g_list_append(ret, panel);
    }

    g_object_unref(parser);
    g_object_unref(reader);

finish:
    g_free(uri);
    g_object_unref(msg);

    return ret;
}


static void
channel_info_async_cb(GTask* task, gpointer source,
    gpointer task_data, GCancellable* cancel)
{
    g_assert(GT_IS_TWITCH(source));
    g_assert(G_IS_TASK(task));
    g_assert_nonnull(task_data);

    GenericTaskData* data = NULL;
    GList* ret = NULL;

    if (g_task_return_error_if_cancelled(task))
        return;

    data = task_data;

    ret = gt_twitch_channel_info(GT_TWITCH(source), data->str_1);

    g_task_return_pointer(task, ret, (GDestroyNotify) gt_twitch_channel_info_panel_list_free);
}

void
gt_twitch_channel_info_async(GtTwitch* self, const gchar* chan,
    GCancellable* cancel, GAsyncReadyCallback cb, gpointer udata)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_false(utils_str_empty(chan));

    GTask* task;
    GenericTaskData* data;

    task = g_task_new(self, cancel, cb, udata);
    g_task_set_return_on_cancel(task, FALSE);

    data = generic_task_data_new();
    data->str_1 = g_strdup(chan);

    g_task_set_task_data(task, data, (GDestroyNotify) generic_task_data_free);

    g_task_run_in_thread(task, channel_info_async_cb);

    g_object_unref(task);
}

GList*
gt_twitch_chat_servers(GtTwitch* self,
    const gchar* chan, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_false(utils_str_empty(chan));

    g_autoptr(SoupMessage) msg;
    g_autoptr(JsonReader) reader;
    g_autofree gchar* uri;
    GList* ret = NULL;
    GError* err = NULL;

    uri = g_strdup_printf(CHAT_SERVERS_URI, chan);

    msg = soup_message_new("GET", uri);

    reader = new_send_message_json(self, msg, &err);

    CHECK_AND_PROPAGATE_ERROR("Unable to fetch chat servers for channel %s", chan);

    READ_JSON_MEMBER("chat_servers");

    for (int i = 0; i < json_reader_count_elements(reader); i++)
    {
        READ_JSON_ELEMENT(i);
        ret = g_list_append(ret, g_strdup(json_reader_get_string_value(reader)));
        END_JSON_ELEMENT();
    }

    END_JSON_MEMBER();

    return ret;

error:
    g_list_free_full(ret, g_free);

    return NULL;
}

static GList*
fetch_followed_streams(GtTwitch* self, const gchar* oauth_token,
    gint limit, gint offset, gint* total, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_cmpint(limit, >, 0);
    g_assert_cmpint(offset, >=, 0);
    g_assert_false(utils_str_empty(oauth_token));

    g_autoptr(SoupMessage) msg;
    g_autoptr(JsonReader) reader;
    g_autofree gchar* uri;
    GList* ret = NULL;
    GError* err = NULL;

    uri = g_strdup_printf(FOLLOWED_STREAMS_URI, limit, offset, oauth_token);

    msg = soup_message_new(SOUP_METHOD_GET, uri);

    reader = new_send_message_json(self, msg, &err);

    CHECK_AND_PROPAGATE_ERROR("Unable to fetch followed streams with oauth token '%s', limit '%d' and offset '%d'",
        oauth_token, limit, offset);

    READ_JSON_VALUE("_total", *total);

    READ_JSON_MEMBER("streams");

    for (gint i = 0; i < json_reader_count_elements(reader); i++)
    {
        GtChannelData* data = NULL;

        READ_JSON_ELEMENT(i);

        data = parse_stream(reader, &err);

        CHECK_AND_PROPAGATE_ERROR("Unable to fetch followed streams with oauth token '%s', limit '%d' and offset '%d'",
            oauth_token, limit, offset);

        ret = g_list_append(ret, data);

        END_JSON_ELEMENT();
    }

    return ret;

error:
    gt_channel_data_list_free(ret);

    return NULL;
}

static GList*
fetch_followed_channels(GtTwitch* self, const gchar* id,
    gint limit, gint offset, gint* total, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_cmpint(limit, >, 0);
    g_assert_cmpint(offset, >=, 0);

    g_autoptr(SoupMessage) msg;
    g_autoptr(JsonReader) reader;
    g_autofree gchar* uri;
    GList* ret = NULL;
    GError* err = NULL;

    uri = g_strdup_printf(FOLLOWED_CHANNELS_URI, id, limit, offset);

    msg = soup_message_new(SOUP_METHOD_GET, uri);

    reader = new_send_message_json(self, msg, &err);

    CHECK_AND_PROPAGATE_ERROR("Unable to fetch followed channels with name '%s', limit '%d' and offset '%d'",
        id, limit, offset);

    READ_JSON_VALUE("_total", *total);

    READ_JSON_MEMBER("follows");

    for (gint i = 0; i < json_reader_count_elements(reader); i++)
    {
        GtChannelData* data = NULL;

        READ_JSON_ELEMENT(i);
        READ_JSON_MEMBER("channel");

        data = parse_channel(reader, &err);

        CHECK_AND_PROPAGATE_ERROR("Unable to fetch followed channels with name '%s', limit '%d' and offset '%d'",
            id, limit, offset);

        ret = g_list_append(ret, data);

        END_JSON_MEMBER();
        END_JSON_ELEMENT();
    }

    return ret;

error:
    gt_channel_data_list_free(ret);

    return NULL;
}

GList*
gt_twitch_fetch_all_followed_channels(GtTwitch* self,
    const gchar* id, const gchar* oauth_token, GError** error)
{
    g_assert(GT_IS_TWITCH(self));

#define LIMIT 100

    gint total = 0;
    GList* chans = NULL;
    GList* streams = NULL;
    GList* ret = NULL;
    GError* err = NULL;

    streams = g_list_concat(streams,
        fetch_followed_streams(self, oauth_token, LIMIT, 0, &total, &err));

    CHECK_AND_PROPAGATE_ERROR("Unable to fetch all followed channels");

    if (total > 100)
    {
        for (gint i = 100; i < total; i += 100)
        {
            streams = g_list_concat(streams,
                fetch_followed_streams(self, oauth_token, LIMIT, 0, NULL, &err));

            CHECK_AND_PROPAGATE_ERROR("Unable to fetch all followed channels");
        }
    }

    chans = g_list_concat(chans,
        fetch_followed_channels(self, id, LIMIT, 0, &total, &err));

    CHECK_AND_PROPAGATE_ERROR("Unable to fetch all followed channels");

    if (total > 100)
    {
        for (gint i = 100; i < total; i += 100)
        {
            chans = g_list_concat(chans,
                fetch_followed_channels(self, id, LIMIT, 0, NULL, &err));

            CHECK_AND_PROPAGATE_ERROR("Unable to fetch all followed channels");
        }
    }

    //NOTE: Remove duplicates
    for (GList* l = streams; l != NULL; l = l->next)
    {
        GList* found = g_list_find_custom(chans, l->data, (GCompareFunc) gt_channel_data_compare);

        if (!found)
        {
            WARNINGF("Unable to fetch all followed channels with id '%s' and oauth token '%s' because: "
                "A followed stream did not exist as a followed channel", id, oauth_token);

            g_set_error(error, GT_TWITCH_ERROR, GT_TWITCH_ERROR_MISC, "Unable to fetch all followed channels with id '%s' and oauth token '%s' because: "
                "A followed stream did not exist as a followed channel", id, oauth_token);

            goto error;
        }
        else
        {
            g_assert_nonnull(found->data);
            g_assert_false(((GtChannelData*) found->data)->online);

            gt_channel_data_free(found->data);

            chans = g_list_delete_link(chans, found);
        }
    }

    chans = g_list_concat(chans, streams);

    for (GList* l = chans; l != NULL; l = l->next)
    {
        GtChannelData* data = l->data;
        GtChannel* chan = gt_channel_new(data);

        ret = g_list_append(ret, chan);
    }

    g_list_free(chans); //NOTE: Don't need to free entire list because gt_channel_new takes ownership of the data

    return ret;

error:
    gt_channel_data_list_free(chans);
    gt_channel_data_list_free(streams);

    return NULL;
}

static void
fetch_all_followed_channels_async_cb(GTask* task, gpointer source,
    gpointer task_data, GCancellable* cancel)
{
    g_assert(GT_IS_TWITCH(source));
    g_assert(G_IS_TASK(task));
    g_assert_nonnull(task_data);

    GenericTaskData* data = task_data;
    GList* ret = NULL;
    GError* err = NULL;

    if (g_task_return_error_if_cancelled(task))
        return;

    ret = gt_twitch_fetch_all_followed_channels(GT_TWITCH(source), data->str_1, data->str_2, &err);

    if (err)
        g_task_return_error(task, err);
    else
        g_task_return_pointer(task, ret, (GDestroyNotify) gt_channel_list_free);
}

void
gt_twitch_fetch_all_followed_channels_async(GtTwitch* self,
    const gchar* id, const gchar* oauth_token, GCancellable* cancel,
    GAsyncReadyCallback cb, gpointer udata)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_false(utils_str_empty(oauth_token));

    GTask* task = NULL;
    GenericTaskData* data = NULL;

    task = g_task_new(self, cancel, cb, udata);
    g_task_set_return_on_cancel(task, FALSE);

    data = generic_task_data_new();
    data->str_1 = g_strdup(id);
    data->str_2 = g_strdup(oauth_token);

    g_task_set_task_data(task, data, (GDestroyNotify) generic_task_data_free);

    g_task_run_in_thread(task, fetch_all_followed_channels_async_cb);

    g_object_unref(task);
}

GList*
gt_twitch_fetch_all_followed_channels_finish(GtTwitch* self,
    GAsyncResult* result, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert(G_IS_ASYNC_RESULT(result));

    GList* ret = g_task_propagate_pointer(G_TASK(result), error);

    return ret;
}

void
gt_twitch_follow_channel(GtTwitch* self,
    const gchar* chan_name, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_false(utils_str_empty(chan_name));

    g_autoptr(SoupMessage) msg = NULL;
    g_autofree gchar* uri = NULL;
    const GtOAuthInfo* oauth_info = NULL;
    GError* err = NULL;

    oauth_info = gt_app_get_oauth_info(main_app);

    uri = g_strdup_printf(FOLLOW_CHANNEL_URI,
        oauth_info->user_name, chan_name, oauth_info->oauth_token);

    msg = soup_message_new(SOUP_METHOD_PUT, uri);

    new_send_message(self, msg, &err);

    CHECK_AND_PROPAGATE_ERROR("Unable to follow channel '%s'",
        chan_name);

error:
    return;
}

static void
follow_channel_async_cb(GTask* task, gpointer source,
    gpointer task_data, GCancellable* cancel)
{
    g_assert(GT_IS_TWITCH(source));
    g_assert(G_IS_TASK(task));
    g_assert_nonnull(task_data);

    GenericTaskData* data = task_data;
    GError* err = NULL;

    gt_twitch_follow_channel(GT_TWITCH(source), data->str_1, &err);

    if (err)
        g_task_return_error(task, err);
    else
        g_task_return_pointer(task, NULL, NULL);
}

// Not cancellable; hard to guarantee that channel is not followed
void
gt_twitch_follow_channel_async(GtTwitch* self, const gchar* chan_name,
    GAsyncReadyCallback cb, gpointer udata)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_false(utils_str_empty(chan_name));

    GTask* task = NULL;
    GenericTaskData* data = NULL;

    task = g_task_new(self, NULL, cb, udata);

    data = generic_task_data_new();
    data->str_1 = g_strdup(chan_name);

    g_task_set_task_data(task, data, (GDestroyNotify) generic_task_data_free);

    g_task_run_in_thread(task, follow_channel_async_cb);

    g_object_unref(task);
}

void
gt_twitch_follow_channel_finish(GtTwitch* self,
    GAsyncResult* result, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert(G_IS_ASYNC_RESULT(result));

    g_task_propagate_pointer(G_TASK(result), error);
}

void
gt_twitch_unfollow_channel(GtTwitch* self,
    const gchar* chan_name, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_false(utils_str_empty(chan_name));

    g_autoptr(SoupMessage) msg = NULL;
    g_autofree gchar* uri = NULL;
    const GtOAuthInfo* oauth_info = NULL;
    GError* err = NULL;

    oauth_info = gt_app_get_oauth_info(main_app);

    uri = g_strdup_printf(UNFOLLOW_CHANNEL_URI,
        oauth_info->user_name, chan_name, oauth_info->oauth_token);

    msg = soup_message_new(SOUP_METHOD_DELETE, uri);

    new_send_message(self, msg, &err);

    CHECK_AND_PROPAGATE_ERROR("Unable to unfollow channel '%s'",
        chan_name);

error:
    return;
}

static void
unfollow_channel_async_cb(GTask* task, gpointer source,
    gpointer task_data, GCancellable* cancel)
{
    g_assert(GT_IS_TWITCH(source));
    g_assert(G_IS_TASK(task));
    g_assert_nonnull(task_data);

    GenericTaskData* data = task_data;
    GError* err = NULL;

    gt_twitch_unfollow_channel(GT_TWITCH(source), data->str_1, &err);

    if (err)
        g_task_return_error(task, err);
    else
        g_task_return_pointer(task, NULL, NULL); //NOTE: Just return null as this is a void function
}

//NOTE: Not cancellable; hard to guarantee that channel is not unfollowed
void
gt_twitch_unfollow_channel_async(GtTwitch* self, const gchar* chan_name,
    GAsyncReadyCallback cb, gpointer udata)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_false(utils_str_empty(chan_name));

    GTask* task = NULL;
    GenericTaskData* data = NULL;

    task = g_task_new(self, NULL, cb, udata);

    data = generic_task_data_new();
    data->str_1 = g_strdup(chan_name);

    g_task_set_task_data(task, data, (GDestroyNotify) generic_task_data_free);

    g_task_run_in_thread(task, unfollow_channel_async_cb);

    g_object_unref(task);
}

void
gt_twitch_unfollow_channel_finish(GtTwitch* self,
    GAsyncResult* result, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert(G_IS_ASYNC_RESULT(result));

    g_task_propagate_pointer(G_TASK(result), error);
}

GList*
gt_twitch_emoticons(GtTwitch* self,
    const gchar* emotesets, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_false(utils_str_empty(emotesets));

    g_autoptr(SoupMessage) msg = NULL;
    g_autoptr(JsonReader) reader = NULL;
    g_autofree gchar* uri = NULL;
    g_auto(GStrv) sets;
    GList* ret = NULL;
    GError* err = NULL;

    uri = g_strdup_printf(EMOTICON_IMAGES_URI,
        emotesets);

    msg = soup_message_new(SOUP_METHOD_GET, uri);

    reader = new_send_message_json(self, msg, &err);

    CHECK_AND_PROPAGATE_ERROR("Unable to get emoticons for emote sets '%s'",
        emotesets);

    sets = g_strsplit(emotesets, ",", 0);

    READ_JSON_MEMBER("emoticon_sets");

    for (gchar** c = sets; *c != NULL; c++)
    {
        READ_JSON_MEMBER(*c);

        for (gint i = 0; i < json_reader_count_elements(reader); i++)
        {
            GtChatEmote* emote = gt_chat_emote_new();

            ret = g_list_append(ret, emote);

            READ_JSON_ELEMENT(i);
            READ_JSON_VALUE("id", emote->id);
            READ_JSON_VALUE("code", emote->code);
            END_JSON_ELEMENT();

            emote->set = atoi(*c);

            emote->pixbuf = gt_twitch_download_emote(self, emote->id);
        }

        END_JSON_MEMBER();
    }

    END_JSON_MEMBER();

    return ret;

error:
    gt_chat_emote_list_free(ret);

    return NULL;
}

static void
emoticon_images_async_cb(GTask* task, gpointer source,
    gpointer task_data, GCancellable* cancel)
{
    g_assert(GT_IS_TWITCH(source));
    g_assert(G_IS_TASK(task));
    g_assert_nonnull(task_data);

    GenericTaskData* data = task_data;
    GList* ret = NULL;
    GError* err = NULL;

    ret = gt_twitch_emoticons(GT_TWITCH(source), data->str_1, &err);

    if (err)
        g_task_return_error(task, err);
    else
        g_task_return_pointer(task, ret, (GDestroyNotify) gt_chat_emote_list_free);
}

void
gt_twitch_emoticons_async(GtTwitch* self, const char* emotesets,
    GAsyncReadyCallback cb, GCancellable* cancel, gpointer udata)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_false(utils_str_empty(emotesets));

    GTask* task = NULL;
    GenericTaskData* data = NULL;

    task = g_task_new(self, cancel, cb, udata);

    data = generic_task_data_new();
    data->str_1 = g_strdup(emotesets);

    g_task_set_task_data(task, data, (GDestroyNotify) generic_task_data_free);

    g_task_run_in_thread(task, emoticon_images_async_cb);

    g_object_unref(task);
}

GtUserInfo*
gt_twitch_fetch_user_info(GtTwitch* self,
    const gchar* oauth_token, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert_false(utils_str_empty(oauth_token));

    g_autoptr(SoupMessage) msg = NULL;
    g_autoptr(JsonReader) reader;
    g_autofree gchar* uri = NULL;
    GtUserInfo* ret = NULL;
    GError* err = NULL;

    uri = g_strdup_printf(USER_INFO_URI, oauth_token);

    msg = soup_message_new(SOUP_METHOD_GET, uri);

    reader = new_send_message_json(self, msg, &err);

    CHECK_AND_PROPAGATE_ERROR("Unable to get user info");

    ret = gt_user_info_new();

    ret->oauth_token = g_strdup(oauth_token);

    READ_JSON_VALUE("_id", ret->id);
    READ_JSON_VALUE("name", ret->name);
    READ_JSON_VALUE_NULL("bio", ret->bio, NULL);
    READ_JSON_VALUE_NULL("display_name", ret->display_name, NULL);
    READ_JSON_VALUE("email", ret->email);
    READ_JSON_VALUE("email_verified", ret->email_verified);
    READ_JSON_VALUE_NULL("logo", ret->logo_url, NULL);
    READ_JSON_MEMBER("notifications");
    READ_JSON_VALUE("email", ret->notifications.email);
    READ_JSON_VALUE("push", ret->notifications.push);
    END_JSON_MEMBER();
    READ_JSON_VALUE("partnered", ret->partnered);
    READ_JSON_VALUE("twitter_connected", ret->twitter_connected);
    READ_JSON_VALUE("type", ret->type);
    READ_JSON_VALUE("created_at", ret->created_at);
    READ_JSON_VALUE("updated_at", ret->updated_at);

    return ret;

error:
    gt_user_info_free(ret);

    return NULL;
}

static void
fetch_user_info_async_cb(GTask* task, gpointer source,
    gpointer task_data, GCancellable* cancel)
{
    g_assert(GT_IS_TWITCH(source));
    g_assert(G_IS_TASK(task));
    g_assert_nonnull(task_data);

    GError* err = NULL;
    GenericTaskData* data = task_data;

    GtUserInfo* ret = gt_twitch_fetch_user_info(GT_TWITCH(source), data->str_1, &err);

    if (err)
        g_task_return_error(task, err);
    else
        g_task_return_pointer(task, ret, (GDestroyNotify) gt_user_info_free);
}

void
gt_twitch_fetch_user_info_async(GtTwitch* self,
    const gchar* oauth_token, GAsyncReadyCallback cb,
    GCancellable* cancel, gpointer udata)
{
    g_assert(GT_IS_TWITCH(self));

    GTask* task = NULL;
    GenericTaskData* data = generic_task_data_new();

    task = g_task_new(self, cancel, cb, udata);

    data->str_1 = g_strdup(oauth_token);

    g_task_set_task_data(task, data, (GDestroyNotify) generic_task_data_free);

    g_task_run_in_thread(task, fetch_user_info_async_cb);

    g_object_unref(task);
}

GtUserInfo*
gt_twitch_fetch_user_info_finish(GtTwitch* self,
    GAsyncResult* result, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert(G_IS_ASYNC_RESULT(result));

    GtUserInfo* ret = g_task_propagate_pointer(G_TASK(result), error);

    return ret;
}

GtOAuthInfo*
gt_twitch_fetch_oauth_info(GtTwitch* self,
    const gchar* oauth_token, GError** error)

{
    g_assert(GT_IS_TWITCH(self));
    g_assert_false(utils_str_empty(oauth_token));

    g_autoptr(SoupMessage) msg = NULL;
    g_autoptr(JsonReader) reader;
    g_autofree gchar* uri = NULL;
    GtOAuthInfo* ret = NULL;
    GError* err = NULL;
    gint num_scopes;

    uri = g_strdup_printf(OAUTH_INFO_URI, oauth_token);

    msg = soup_message_new(SOUP_METHOD_GET, uri);

    reader = new_send_message_json(self, msg, &err);

    CHECK_AND_PROPAGATE_ERROR("Unable to get oauth info");

    ret = gt_oauth_info_new();

    ret->oauth_token = g_strdup(oauth_token);

    READ_JSON_MEMBER("token");
    READ_JSON_VALUE("user_id", ret->user_id);
    READ_JSON_VALUE("user_name", ret->user_name);
    READ_JSON_VALUE("client_id", ret->client_id);
    READ_JSON_VALUE("valid", ret->valid);
    READ_JSON_MEMBER("authorization");
    READ_JSON_VALUE("created_at", ret->created_at);
    READ_JSON_VALUE("created_at", ret->updated_at);
    READ_JSON_MEMBER("scopes");
    num_scopes = json_reader_count_elements(reader);
    ret->scopes = g_malloc_n(num_scopes + 1, sizeof(gchar*));
    for (gint i = 0; i < num_scopes; i++)
    {
        READ_JSON_ELEMENT_VALUE(i, *(ret->scopes + i));
    }
    *(ret->scopes + num_scopes) = NULL;
    END_JSON_MEMBER();
    END_JSON_MEMBER();
    END_JSON_MEMBER();

    return ret;

error:
    gt_oauth_info_free(ret);

    return NULL;
}

static void
fetch_oauth_info_async_cb(GTask* task, gpointer source,
    gpointer task_data, GCancellable* cancel)
{
    g_assert(GT_IS_TWITCH(source));
    g_assert(G_IS_TASK(task));
    g_assert_nonnull(task_data);

    GError* err = NULL;
    GenericTaskData* data = task_data;

    GtOAuthInfo* ret = gt_twitch_fetch_oauth_info(GT_TWITCH(source), data->str_1, &err);

    if (err)
        g_task_return_error(task, err);
    else
        g_task_return_pointer(task, ret, (GDestroyNotify) gt_oauth_info_free);
}

void
gt_twitch_fetch_oauth_info_async(GtTwitch* self,
    const gchar* oauth_token, GAsyncReadyCallback cb,
    GCancellable* cancel, gpointer udata)
{
    g_assert(GT_IS_TWITCH(self));

    GTask* task = NULL;
    GenericTaskData* data = generic_task_data_new();

    task = g_task_new(self, cancel, cb, udata);

    data->str_1 = g_strdup(oauth_token);

    g_task_set_task_data(task, data, (GDestroyNotify) generic_task_data_free);

    g_task_run_in_thread(task, fetch_oauth_info_async_cb);

    g_object_unref(task);
}

GtOAuthInfo*
gt_twitch_fetch_oauth_info_finish(GtTwitch* self,
    GAsyncResult* result, GError** error)
{
    g_assert(GT_IS_TWITCH(self));
    g_assert(G_IS_ASYNC_RESULT(result));

    GtOAuthInfo* ret = g_task_propagate_pointer(G_TASK(result), error);

    return ret;
}

GtChatBadge*
gt_chat_badge_new()
{
    return g_slice_new0(GtChatBadge);
}

void
gt_chat_badge_free(GtChatBadge* badge)
{
    g_assert_nonnull(badge);

    g_free(badge->name);
    g_free(badge->version);
    g_object_unref(badge->pixbuf);
    g_slice_free(GtChatBadge, badge);
}

void
gt_chat_badge_list_free(GList* list)
{
    g_list_free_full(list, (GDestroyNotify) gt_chat_badge_free);
}
