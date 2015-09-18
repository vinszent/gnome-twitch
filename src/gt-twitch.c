#include "gt-twitch.h"
#include "gt-twitch-channel.h"
#include "gt-twitch-game.h"
#include <libsoup/soup.h>
#include <glib/gprintf.h>
#include <json-glib/json-glib.h>
#include <string.h>
#include <stdlib.h>

#define ACCESS_TOKEN_URI    "http://api.twitch.tv/api/channels/%s/access_token"
#define STREAM_PLAYLIST_URI "http://usher.twitch.tv/api/channel/hls/%s.m3u8?player=twitchweb&token=%s&sig=%s&allow_audio_only=true&allow_source=true&type=any&p=%d"
#define TOP_CHANNELS_URI    "https://api.twitch.tv/kraken/streams?limit=%d&offset=%d&game=%s"
#define TOP_GAMES_URI       "https://api.twitch.tv/kraken/games/top?limit=%d&offset=%d"
#define SEARCH_CHANNELS_URI "https://api.twitch.tv/kraken/search/streams?q=%s&limit=%d&offset=%d&hls=true"
#define SEARCH_GAMES_URI    "https://api.twitch.tv/kraken/search/games?q=%s&type=suggest"
#define STREAMS_URI         "https://api.twitch.tv/kraken/streams/%s"

#define STREAM_INFO "#EXT-X-STREAM-INF"

typedef struct
{
    SoupSession* soup;
} GtTwitchPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtTwitch, gt_twitch,  G_TYPE_OBJECT)

enum 
{
    PROP_0,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

typedef struct
{
    GtTwitch* twitch;

    gint64 int_1;
    gint64 int_2;
    gint64 int_3;

    gchar* str_1;
    gchar* str_2;
    gchar* str_3;
} GenericTaskData;

static void
generic_task_data_free(GenericTaskData* data)
{
    g_free(data->str_1);
    g_free(data->str_2);
    g_free(data->str_3);
    g_free(data);
}

GtTwitch*
gt_twitch_new(void)
{
    return g_object_new(GT_TYPE_TWITCH, 
                        NULL);
}


static void
finalize(GObject* object)
{
    GtTwitch* self = (GtTwitch*) object;
    GtTwitchPrivate* priv = gt_twitch_get_instance_private(self);

    G_OBJECT_CLASS(gt_twitch_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtTwitch* self = GT_TWITCH(obj);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
set_property(GObject*      obj,
             guint         prop,
             const GValue* val,
             GParamSpec*   pspec)
{
    GtTwitch* self = GT_TWITCH(obj);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_twitch_class_init(GtTwitchClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;
}

static void
gt_twitch_init(GtTwitch* self)
{
    GtTwitchPrivate* priv = gt_twitch_get_instance_private(self);

    priv->soup = soup_session_new();
}

static void
send_message(GtTwitch* self, SoupMessage* msg)
{
    GtTwitchPrivate* priv = gt_twitch_get_instance_private(self);

    soup_session_send_message(priv->soup, msg);

    /* g_print("\n\n%s\n\n", msg->response_body->data); */
}

static GdkPixbuf*
download_picture(GtTwitch* self, const gchar* url)
{
    GtTwitchPrivate* priv = gt_twitch_get_instance_private(self);
    SoupMessage* msg;
    GdkPixbufLoader* loader;
    GdkPixbuf* ret;
    GInputStream* input;

    msg = soup_message_new("GET", url);
    input = soup_session_send(priv->soup, msg, NULL, NULL);

    ret = gdk_pixbuf_new_from_stream(input, NULL, NULL);
    g_object_force_floating(G_OBJECT(ret));

    g_input_stream_close(input, NULL, NULL);
    g_object_unref(msg);

    return ret;
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
    gchar** lines = g_strsplit(playlist, "\n", 0);

    for (gchar** l = lines; *l != NULL; l++)
    {
        if (strncmp(*l, STREAM_INFO, strlen(STREAM_INFO)) == 0)
        {
            GtTwitchStreamData* stream = g_malloc0(sizeof(GtTwitchStreamData));
            ret = g_list_append(ret, stream);

            gchar** values = g_strsplit(*l, ",", 0);

            for (gchar** m = values; *m != NULL; m++)
            {
                gchar** split = g_strsplit(*m, "=", 0);

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
                    if (strcmp(*(split+1), "\"chunked\"") == 0)
                        stream->quality = GT_TWITCH_STREAM_QUALITY_SOURCE;
                    else if (strcmp(*(split+1), "\"high\"") == 0)
                        stream->quality = GT_TWITCH_STREAM_QUALITY_HIGH;
                    else if (strcmp(*(split+1), "\"medium\"") == 0)
                        stream->quality = GT_TWITCH_STREAM_QUALITY_MEDIUM;
                    else if (strcmp(*(split+1), "\"low\"") == 0)
                        stream->quality = GT_TWITCH_STREAM_QUALITY_LOW;
                    else if (strcmp(*(split+1), "\"mobile\"") == 0)
                        stream->quality = GT_TWITCH_STREAM_QUALITY_MOBILE;
                    else if (strcmp(*(split+1), "\"audio_only\"") == 0)
                        stream->quality = GT_TWITCH_STREAM_QUALITY_AUDIO_ONLY;
                }

                g_strfreev(split);
            }
            
            l++;

            stream->url = g_strdup(*l);

            g_strfreev(values);
        }
    }

    g_strfreev(lines);

    return ret;
}

static GtTwitchChannelRawData*
parse_channel(GtTwitch* self, JsonReader* reader)
{
    GtTwitchChannelRawData* ret = g_malloc0(sizeof(GtTwitchChannelRawData));

    json_reader_read_member(reader, "game");
    if (!json_reader_get_null_value(reader))
        ret->game = g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    json_reader_read_member(reader, "viewers");
    ret->viewers = json_reader_get_int_value(reader);
    json_reader_end_member(reader);

    json_reader_read_member(reader, "created_at");
    ret->stream_started_time = parse_time(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    json_reader_read_member(reader, "channel");

    json_reader_read_member(reader, "name");
    ret->name = g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    json_reader_read_member(reader, "display_name");
    ret->display_name = g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    json_reader_read_member(reader, "status");
    ret->status = g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    json_reader_end_member(reader);

    json_reader_read_member(reader, "preview");

    json_reader_read_member(reader, "large");
    ret->preview = download_picture(self, json_reader_get_string_value(reader));
    g_object_ref_sink(ret->preview);
    json_reader_end_member(reader);

    json_reader_end_member(reader);

    return ret;
}

void
gt_twitch_stream_access_token(GtTwitch* self, gchar* channel, gchar** token, gchar** sig)
{
    GtTwitchPrivate* priv = gt_twitch_get_instance_private(self);
    SoupMessage* msg;
    gchar* uri;
    JsonParser* parser;
    JsonNode* node;
    JsonReader* reader;

    uri = g_strdup_printf(ACCESS_TOKEN_URI, channel);
    msg = soup_message_new("GET", uri);

    send_message(self, msg);

    parser = json_parser_new();

    json_parser_load_from_data(parser, msg->response_body->data, msg->response_body->length, NULL);
    node = json_parser_get_root(parser);
    reader = json_reader_new(node);

    json_reader_read_member(reader, "sig");
    *sig = g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    json_reader_read_member(reader, "token");
    *token = g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    g_free(uri);
    g_object_unref(msg);
    g_object_unref(reader);
    g_object_unref(parser);
}

GList*
gt_twitch_all_streams(GtTwitch* self, gchar* channel, gchar* token, gchar* sig)
{
    GtTwitchPrivate* priv = gt_twitch_get_instance_private(self);
    SoupMessage* msg;
    gchar* uri;
    GList* ret;

    uri = g_strdup_printf(STREAM_PLAYLIST_URI, channel, token, sig, g_random_int_range(0, 999999));
    msg = soup_message_new("GET", uri);

    send_message(self, msg);

    ret = parse_playlist(msg->response_body->data);

    g_object_unref(msg);
    g_free(uri);

    return ret;
}

GtTwitchStreamData* 
gt_twitch_stream_by_quality(GtTwitch* self, 
                            gchar* channel, 
                            GtTwitchStreamQuality qual, 
                            gchar* token, gchar* sig)
{
    GList* channels = gt_twitch_all_streams(self, channel, token, sig);

    for (GList* l = channels; l != NULL; l = l->next)
    {
        GtTwitchStreamData* s = (GtTwitchStreamData*) l->data;

        if (s->quality == qual)
            return s;
    }

    //TODO: Free rest of channels

    return NULL;
}

GList*
gt_twitch_top_channels(GtTwitch* self, gint n, gint offset, gchar* game)
{
    GtTwitchPrivate* priv = gt_twitch_get_instance_private(self);
    SoupMessage* msg;
    gchar* uri;
    JsonParser* parser;
    JsonNode* node;
    JsonReader* reader;
    JsonArray* channels;
    GList* ret = NULL;

    uri = g_strdup_printf(TOP_CHANNELS_URI, n, offset, game); //TODO: Add game argument
    msg = soup_message_new("GET", uri);

    send_message(self, msg);

    parser = json_parser_new();
    json_parser_load_from_data(parser, msg->response_body->data, msg->response_body->length, NULL);
    node = json_parser_get_root(parser);
    reader = json_reader_new(node);

    json_reader_read_member(reader, "streams");

    for (gint i = 0; i < json_reader_count_elements(reader); i++)
    {
        GtTwitchChannel* channel;

        json_reader_read_element(reader, i);

        json_reader_read_member(reader, "_id");
        channel = gt_twitch_channel_new(json_reader_get_int_value(reader));
        g_object_force_floating(G_OBJECT(channel));
        json_reader_end_member(reader);

        json_reader_read_member(reader, "viewers");
        g_object_set(channel, "viewers", json_reader_get_int_value(reader), NULL);
        json_reader_end_member(reader);

        json_reader_read_member(reader, "created_at");
        g_object_set(channel, "created_at", parse_time(json_reader_get_string_value(reader)), NULL);
        json_reader_end_member(reader);

        json_reader_read_member(reader, "channel");

        json_reader_read_member(reader, "status");
        if (!json_reader_get_null_value(reader)) //TODO: Does this need to be done for all values?
            g_object_set(channel, "status", json_reader_get_string_value(reader), NULL);
        json_reader_end_member(reader);

        json_reader_read_member(reader, "name");
        g_object_set(channel, "name", json_reader_get_string_value(reader), NULL);
        json_reader_end_member(reader);

        json_reader_read_member(reader, "display_name");
        g_object_set(channel, "display-name", json_reader_get_string_value(reader), NULL);
        json_reader_end_member(reader);

        json_reader_read_member(reader, "game");
        if (!json_reader_get_null_value(reader)) //TODO: Does this need to be done for all values?
            g_object_set(channel, "game", json_reader_get_string_value(reader), NULL);
        json_reader_end_member(reader);

        json_reader_end_member(reader);

        json_reader_read_member(reader, "preview");
        /* json_reader_read_member(reader, "small"); */
        /* g_object_set(channel, "preview-small", download_picture(self, json_reader_get_string_value(reader)), NULL); */
        /* json_reader_end_member(reader); */

        json_reader_read_member(reader, "large");
        g_object_set(channel, "preview", download_picture(self, json_reader_get_string_value(reader)), NULL);
        json_reader_end_member(reader);

        /* json_reader_read_member(reader, "large"); */
        /* g_object_set(channel, "preview-large", download_picture(self, json_reader_get_string_value(reader)), NULL); */
        /* json_reader_end_member(reader); */
        json_reader_end_member(reader);

        json_reader_end_element(reader);

        ret = g_list_append(ret, channel);
    }
    json_reader_end_member(reader);

    g_object_unref(msg);
    g_object_unref(parser);
    g_object_unref(reader);
    g_free(uri);

    return ret;
}

GList* 
gt_twitch_top_games(GtTwitch* self,
                    gint n, gint offset)
{
    GtTwitchPrivate* priv = gt_twitch_get_instance_private(self);
    SoupMessage* msg;
    gchar* uri;
    JsonParser* parser;
    JsonNode* node;
    JsonReader* reader;
    JsonArray* channels;
    GList* ret = NULL;

    uri = g_strdup_printf(TOP_GAMES_URI, n, offset);
    msg = soup_message_new("GET", uri);

    send_message(self, msg);

    parser = json_parser_new();
    json_parser_load_from_data(parser, msg->response_body->data, msg->response_body->length, NULL);
    node = json_parser_get_root(parser);
    reader = json_reader_new(node);

    json_reader_read_member(reader, "top");

    for (gint i = 0; i < json_reader_count_elements(reader); i++)
    {
        GtTwitchGame* game;

        json_reader_read_element(reader, i);

        json_reader_read_member(reader, "game");

        json_reader_read_member(reader, "_id");
        game = gt_twitch_game_new(json_reader_get_int_value(reader));
        g_object_force_floating(G_OBJECT(game));
        json_reader_end_member(reader);

        json_reader_read_member(reader, "name");
        g_object_set(game, "name", json_reader_get_string_value(reader), NULL);
        json_reader_end_member(reader);

        json_reader_read_member(reader, "box");
        /* json_reader_read_member(reader, "small"); */
        /* g_object_set(game, "preview-small", download_picture(self, json_reader_get_string_value(reader)), NULL); */
        /* json_reader_end_member(reader); */

        /* json_reader_read_member(reader, "medium"); */
        /* g_object_set(game, "preview-medium", download_picture(self, json_reader_get_string_value(reader)), NULL); */
        /* json_reader_end_member(reader); */

        json_reader_read_member(reader, "large");
        g_object_set(game, "preview", download_picture(self, json_reader_get_string_value(reader)), NULL);
        json_reader_end_member(reader);
        json_reader_end_member(reader);

        /* json_reader_read_member(reader, "logo"); */
        /* json_reader_read_member(reader, "small"); */
        /* g_object_set(game, "logo-small", download_picture(self, json_reader_get_string_value(reader)), NULL); */
        /* json_reader_end_member(reader); */

        /* json_reader_read_member(reader, "medium"); */
        /* g_object_set(game, "logo-medium", download_picture(self, json_reader_get_string_value(reader)), NULL); */
        /* json_reader_end_member(reader); */

        /* json_reader_read_member(reader, "large"); */
        /* g_object_set(game, "logo-large", download_picture(self, json_reader_get_string_value(reader)), NULL); */
        /* json_reader_end_member(reader); */

        /* json_reader_end_member(reader); */

        json_reader_end_member(reader);

        json_reader_read_member(reader, "viewers");
        g_object_set(game, "viewers", json_reader_get_int_value(reader), NULL);
        json_reader_end_member(reader);

        json_reader_read_member(reader, "channels");
        g_object_set(game, "channels", json_reader_get_int_value(reader), NULL);
        json_reader_end_member(reader);

        json_reader_end_element(reader);

        ret = g_list_append(ret, game);
    }

    json_reader_end_member(reader);

    g_object_unref(msg);
    g_object_unref(parser);
    g_object_unref(reader);
    g_free(uri);

    return ret;
}

typedef struct
{
    GtTwitch* twitch;
    gint n;
    gint offset;
    gchar* game;
} TopDataClosure;

static void
top_channels_async_cb(GTask* task,
                      gpointer source,
                      gpointer task_data,
                      GCancellable* cancel)
{
    TopDataClosure* data = task_data;
    GList* ret;

    if (g_task_return_error_if_cancelled(task))
        return;

    ret = gt_twitch_top_channels(data->twitch, data->n, data->offset, data->game);

    g_task_return_pointer(task, ret, (GDestroyNotify) gt_twitch_channel_free_list);
}

GList*
gt_twitch_top_channels_async(GtTwitch* self, gint n, gint offset, gchar* game,
                             GCancellable* cancel,
                             GAsyncReadyCallback cb,
                             gpointer udata)
{
    GTask* task = NULL;
    TopDataClosure* data = NULL;

    task = g_task_new(NULL, cancel, cb, udata);
    g_task_set_return_on_cancel(task, FALSE);

    data = g_new0(TopDataClosure, 1);
    data->twitch = self;
    data->n = n;
    data->offset = offset;
    data->game = g_strdup(game);

    g_task_set_task_data(task, data, g_free);

    g_task_run_in_thread(task, top_channels_async_cb);

    g_object_unref(task);
}

static void
top_games_async_cb(GTask* task,
                   gpointer source,
                   gpointer task_data,
                   GCancellable* cancel)
{
    TopDataClosure* data = task_data;
    GList* ret;

    if (g_task_return_error_if_cancelled(task))
        return;

    ret = gt_twitch_top_games(data->twitch, data->n, data->offset);

    g_task_return_pointer(task, ret, (GDestroyNotify) gt_twitch_game_free_list);
}

GList*
gt_twitch_top_games_async(GtTwitch* self, gint n, gint offset,
                             GCancellable* cancel,
                             GAsyncReadyCallback cb,
                             gpointer udata)
{
    GTask* task = NULL;
    TopDataClosure* data = NULL;

    task = g_task_new(NULL, cancel, cb, udata);
    g_task_set_return_on_cancel(task, FALSE);

    data = g_new0(TopDataClosure, 1);
    data->twitch = self;
    data->n = n;
    data->offset = offset;

    g_task_set_task_data(task, data, g_free);

    g_task_run_in_thread(task, top_games_async_cb);

    g_object_unref(task);
}

GList*
gt_twitch_search_channels(GtTwitch* self, gchar* query, gint n, gint offset)
{
    GtTwitchPrivate* priv = gt_twitch_get_instance_private(self);
    SoupMessage* msg;
    gchar* uri;
    JsonParser* parser;
    JsonNode* node;
    JsonReader* reader;
    JsonArray* channels;
    GList* ret = NULL;

    uri = g_strdup_printf(SEARCH_CHANNELS_URI, query, n, offset);
    msg = soup_message_new("GET", uri);

    send_message(self, msg);

    parser = json_parser_new();
    json_parser_load_from_data(parser, msg->response_body->data, msg->response_body->length, NULL);
    node = json_parser_get_root(parser);
    reader = json_reader_new(node);

    json_reader_read_member(reader, "streams");
    for (gint i = 0; i < json_reader_count_elements(reader); i++)
    {
        GtTwitchChannel* channel;

        json_reader_read_element(reader, i);

        json_reader_read_member(reader, "_id");
        channel = gt_twitch_channel_new(json_reader_get_int_value(reader));
        g_object_force_floating(G_OBJECT(channel));
        json_reader_end_member(reader);

        json_reader_read_member(reader, "viewers");
        g_object_set(channel, "viewers", json_reader_get_int_value(reader), NULL);
        json_reader_end_member(reader);

        json_reader_read_member(reader, "created_at");
        g_object_set(channel, "created-at", parse_time(json_reader_get_string_value(reader)), NULL);
        json_reader_end_member(reader);

        json_reader_read_member(reader, "channel");

        json_reader_read_member(reader, "status");
        g_object_set(channel, "status", json_reader_get_string_value(reader), NULL);
        json_reader_end_member(reader);

        json_reader_read_member(reader, "name");
        g_object_set(channel, "name", json_reader_get_string_value(reader), NULL);
        json_reader_end_member(reader);

        json_reader_read_member(reader, "display_name");
        g_object_set(channel, "display-name", json_reader_get_string_value(reader), NULL);
        json_reader_end_member(reader);

        json_reader_read_member(reader, "game");
        if (!json_reader_get_null_value(reader))
            g_object_set(channel, "game", json_reader_get_string_value(reader), NULL);
        json_reader_end_member(reader);

        json_reader_end_member(reader);

        json_reader_read_member(reader, "preview");
        /* json_reader_read_member(reader, "small"); */
        /* g_object_set(channel, "preview-small", download_picture(self, json_reader_get_string_value(reader)), NULL); */
        /* json_reader_end_member(reader); */

        json_reader_read_member(reader, "large");
        g_object_set(channel, "preview", download_picture(self, json_reader_get_string_value(reader)), NULL);
        json_reader_end_member(reader);

        /* json_reader_read_member(reader, "large"); */
        /* g_object_set(channel, "preview-large", download_picture(self, json_reader_get_string_value(reader)), NULL); */
        /* json_reader_end_member(reader); */
        json_reader_end_member(reader);

        json_reader_end_element(reader);

        ret = g_list_append(ret, channel);
    }
    json_reader_end_member(reader);

    g_object_unref(msg);
    g_object_unref(parser);
    g_object_unref(reader);
    g_free(uri);

    return ret;
}

GList*
gt_twitch_search_games(GtTwitch* self, gchar* query, gint n, gint offset)
{
    GtTwitchPrivate* priv = gt_twitch_get_instance_private(self);
    SoupMessage* msg;
    gchar* uri;
    JsonParser* parser;
    JsonNode* node;
    JsonReader* reader;
    JsonArray* channels;
    GList* ret = NULL;

    uri = g_strdup_printf(SEARCH_GAMES_URI, query);
    msg = soup_message_new("GET", uri);

    send_message(self, msg);

    parser = json_parser_new();
    json_parser_load_from_data(parser, msg->response_body->data, msg->response_body->length, NULL);
    node = json_parser_get_root(parser);
    reader = json_reader_new(node);

    json_reader_read_member(reader, "games");
    for (gint i = 0; i < json_reader_count_elements(reader); i++)
    {
        GtTwitchGame* game;

        json_reader_read_element(reader, i);

        json_reader_read_member(reader, "_id");
        game = gt_twitch_game_new(json_reader_get_int_value(reader));
        g_object_force_floating(G_OBJECT(game));
        json_reader_end_member(reader);

        json_reader_read_member(reader, "name");
        g_object_set(game, "name", json_reader_get_string_value(reader), NULL);
        json_reader_end_member(reader);

        json_reader_read_member(reader, "box");
        /* json_reader_read_member(reader, "small"); */
        /* g_object_set(game, "preview-small", download_picture(self, json_reader_get_string_value(reader)), NULL); */
        /* json_reader_end_member(reader); */

        /* json_reader_read_member(reader, "medium"); */
        /* g_object_set(game, "preview-medium", download_picture(self, json_reader_get_string_value(reader)), NULL); */
        /* json_reader_end_member(reader); */

        json_reader_read_member(reader, "large");
        g_object_set(game, "preview", download_picture(self, json_reader_get_string_value(reader)), NULL);
        json_reader_end_member(reader);
        json_reader_end_member(reader);

        /* json_reader_read_member(reader, "logo"); */
        /* json_reader_read_member(reader, "small"); */
        /* g_object_set(game, "logo-small", download_picture(self, json_reader_get_string_value(reader)), NULL); */
        /* json_reader_end_member(reader); */

        /* json_reader_read_member(reader, "medium"); */
        /* g_object_set(game, "logo-medium", download_picture(self, json_reader_get_string_value(reader)), NULL); */
        /* json_reader_end_member(reader); */

        /* json_reader_read_member(reader, "large"); */
        /* g_object_set(game, "logo-large", download_picture(self, json_reader_get_string_value(reader)), NULL); */
        /* json_reader_end_member(reader); */
        /* json_reader_end_member(reader); */

        json_reader_end_element(reader);

        ret = g_list_append(ret, game);
    }
    json_reader_end_member(reader);

    g_object_unref(msg);
    g_object_unref(parser);
    g_object_unref(reader);
    g_free(uri);

    return ret;
}

typedef struct
{
    GtTwitch* twitch;
    gint n;
    gint offset;
    gchar* query;
} SearchDataClosure;

static void
free_search_data_closure(SearchDataClosure* data)
{
    g_free(data->query);
    g_free(data);
}

static void
search_channels_async_cb(GTask* task,
                         gpointer source,
                         gpointer task_data,
                         GCancellable* cancel)
{
    SearchDataClosure* data = task_data;
    GList* ret;

    if (g_task_return_error_if_cancelled(task))
        return;

    ret = gt_twitch_search_channels(data->twitch, data->query, data->n, data->offset);

    g_task_return_pointer(task, ret, (GDestroyNotify) gt_twitch_channel_free_list);
}

void
gt_twitch_search_channels_async(GtTwitch* self, gchar* query, 
                                gint n, gint offset,
                                GCancellable* cancel,
                                GAsyncReadyCallback cb,
                                gpointer udata)
{
    GTask* task = NULL;
    SearchDataClosure* data = NULL;

    task = g_task_new(NULL, cancel, cb, udata);
    g_task_set_return_on_cancel(task, FALSE);

    data = g_new0(SearchDataClosure, 1);
    data->twitch = self;
    data->n = n;
    data->offset = offset;
    data->query = g_strdup(query);

    g_task_set_task_data(task, data, (GDestroyNotify) free_search_data_closure);

    g_task_run_in_thread(task, search_channels_async_cb);

    g_object_unref(task);
}

static void
search_games_async_cb(GTask* task,
                         gpointer source,
                         gpointer task_data,
                         GCancellable* cancel)
{
    SearchDataClosure* data = task_data;
    GList* ret;

    if (g_task_return_error_if_cancelled(task))
        return;

    ret = gt_twitch_search_games(data->twitch, data->query, data->n, data->offset);

    g_task_return_pointer(task, ret, (GDestroyNotify) gt_twitch_game_free_list);
}

void
gt_twitch_search_games_async(GtTwitch* self, gchar* query, 
                                gint n, gint offset,
                                GCancellable* cancel,
                                GAsyncReadyCallback cb,
                                gpointer udata)
{
    GTask* task = NULL;
    SearchDataClosure* data = NULL;

    task = g_task_new(NULL, cancel, cb, udata);
    g_task_set_return_on_cancel(task, FALSE);

    data = g_new0(SearchDataClosure, 1);
    data->twitch = self;
    data->n = n;
    data->offset = offset;
    data->query = g_strdup(query);

    g_task_set_task_data(task, data, (GDestroyNotify) free_search_data_closure);

    g_task_run_in_thread(task, search_games_async_cb);

    g_object_unref(task);
}

void
gt_twitch_stream_free(GtTwitchStreamData* channel)
{
    g_free(channel->url);

    g_free(channel);
}

GtTwitchChannelRawData*
gt_twitch_channel_raw_data(GtTwitch* self, const gchar* name)
{
    GtTwitchPrivate* priv = gt_twitch_get_instance_private(self);
    SoupMessage* msg;
    gchar* uri;
    JsonParser* parser;
    JsonNode* node;
    JsonReader* reader;
    GtTwitchChannelRawData* ret;

    uri = g_strdup_printf(STREAMS_URI, name);
    msg = soup_message_new("GET", uri);

    send_message(self, msg);

    parser = json_parser_new();
    json_parser_load_from_data(parser, msg->response_body->data, msg->response_body->length, NULL);
    node = json_parser_get_root(parser);
    reader = json_reader_new(node);

    json_reader_read_member(reader, "stream");

    ret = parse_channel(self, reader);

    json_reader_end_member(reader);

    return ret;
}


static void
channel_raw_data_cb(GTask* task,
                    gpointer source,
                    gpointer task_data,
                    GCancellable* cancel)
{
    GtTwitchChannelRawData* ret;
    GenericTaskData* data = task_data;

    if (g_task_return_error_if_cancelled(task))
        return;

    ret = gt_twitch_channel_raw_data(data->twitch, data->str_1);

    g_task_return_pointer(task, ret, (GDestroyNotify) gt_twitch_channel_raw_data_free);
}

// This was a bit unecessary, but at least it's here if it's ever needed
void
gt_twitch_channel_raw_data_async(GtTwitch* self, const gchar* name,
                          GCancellable* cancel,
                          GAsyncReadyCallback cb,
                          gpointer udata)
{
    GTask* task = NULL;
    GenericTaskData* data;

    task = g_task_new(NULL, cancel, cb, udata);
    g_task_set_return_on_cancel(task, FALSE);

    data = g_new0(GenericTaskData, 1);
    data->twitch = self;
    data->str_1 = g_strdup(name);
    g_task_set_task_data(task, data, (GDestroyNotify) generic_task_data_free);

    g_task_run_in_thread(task, channel_raw_data_cb);

    g_object_unref(task);
}

void gt_twitch_channel_raw_data_free(GtTwitchChannelRawData* data)
{
    g_free(data->game);
    g_free(data->status);
    g_free(data->name);
    g_free(data->display_name);
    g_date_time_unref(data->stream_started_time);
    g_object_unref(data->preview);
    g_free(data);
}
