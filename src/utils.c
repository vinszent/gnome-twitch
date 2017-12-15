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

#include <glib/gstdio.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "config.h"
#include "gt-win.h"

#define TAG "Utils"
#include "gnome-twitch/gt-log.h"

gpointer
utils_value_ref_sink_object(const GValue* val)
{
    if (val == NULL || !G_VALUE_HOLDS_OBJECT(val) || g_value_get_object(val) == NULL)
        return NULL;
    else
        return g_object_ref_sink(g_value_get_object(val));
}

gchar*
utils_value_dup_string_allow_null(const GValue* val)
{
    if (g_value_get_string(val))
        return g_value_dup_string(val);

    return NULL;
}

void
utils_container_clear(GtkContainer* cont)
{
    for(GList* l = gtk_container_get_children(cont);
        l != NULL; l = l->next)
    {
        gtk_container_remove(cont, GTK_WIDGET(l->data));
    }
}

guint64
utils_timestamp_filename(const gchar* filename, GError** error)
{
    RETURN_VAL_IF_FAIL(!utils_str_empty(filename), 0);

    GError* err = NULL; /* NOTE: Doesn't need to be freed because we propagate it */

    if (g_file_test(filename, G_FILE_TEST_EXISTS))
    {
        GTimeVal time;

        g_autoptr(GFile) file = g_file_new_for_path(filename);

        return utils_timestamp_file(file, error);
    }

    RETURN_VAL_IF_REACHED(0);
}

guint64
utils_timestamp_file(GFile* file, GError** error)
{
    g_autoptr(GFileInfo) info = g_file_query_info(file,
        G_FILE_ATTRIBUTE_TIME_MODIFIED, G_FILE_QUERY_INFO_NONE,
        NULL, error);
    GTimeVal time;

    if (error)
    {
        g_prefix_error(error, "Could not timestamp file because: ");

        WARNING("%s", (*error)->message);

        return 0;
    }

    g_file_info_get_modification_time(info, &time);

    return time.tv_sec;
}

gint64
utils_timestamp_now(void)
{
    gint64 timestamp;
    GDateTime* now;

    now = g_date_time_new_now_utc();
    timestamp = g_date_time_to_unix(now);
    g_date_time_unref(now);

    return timestamp;
}

void
utils_pixbuf_scale_simple(GdkPixbuf** pixbuf, gint width, gint height, GdkInterpType interp)
{
    if (!*pixbuf)
        return;

    GdkPixbuf* tmp = gdk_pixbuf_scale_simple(*pixbuf, width, height, interp);
    g_clear_object(pixbuf);
    *pixbuf = tmp;
}

guint64
utils_http_full_date_to_timestamp(const char* string)
{
    gint64 ret = G_MAXINT64;
    g_autoptr(SoupDate) date = NULL;

    date = soup_date_new_from_string(string);

    RETURN_VAL_IF_FAIL(date != NULL, ret);

    ret = soup_date_to_time_t(date);

    return ret;
}

const gchar*
utils_search_key_value_strv(gchar** strv, const gchar* key)
{
    if (!strv)
        return NULL;

    for (gchar** s = strv; *s != NULL; s += 2)
    {
        if (g_strcmp0(*s, key) == 0)
            return *(s+1);
    }

    return NULL;
}

static gboolean
utils_mouse_hover_enter_cb(GtkWidget* widget,
                           GdkEvent* evt,
                           gpointer udata)
{
    GdkWindow* win;
    GdkDisplay* disp;
    GdkCursor* cursor;

    win = ((GdkEventMotion*) evt)->window;
    disp = gdk_window_get_display(win);
    cursor = gdk_cursor_new_for_display(disp, GDK_HAND2);

    gdk_window_set_cursor(win, cursor);

    g_object_unref(cursor);

    return FALSE;
}

static gboolean
utils_mouse_hover_leave_cb(GtkWidget* widget,
                           GdkEvent* evt,
                           gpointer udata)
{
    GdkWindow* win;
    GdkDisplay* disp;
    GdkCursor* cursor;

    win = ((GdkEventMotion*) evt)->window;
    disp = gdk_window_get_display(win);
    cursor = gdk_cursor_new_for_display(disp, GDK_LEFT_PTR);

    gdk_window_set_cursor(win, cursor);

    g_object_unref(cursor);

    return FALSE;
}

static gboolean
utils_mouse_clicked_link_cb(GtkWidget* widget,
                            GdkEventButton* evt,
                            gpointer udata)
{
    if (evt->button == 1 && evt->type == GDK_BUTTON_PRESS)
    {
        GtWin* parent = GT_WIN_TOPLEVEL(widget);

#if GTK_CHECK_VERSION(3, 22, 0)
        gtk_show_uri_on_window(GTK_WINDOW(parent), (gchar*) udata, GDK_CURRENT_TIME, NULL);
#else
        gtk_show_uri(NULL, (gchar*) udata, GDK_CURRENT_TIME, NULL);
#endif

    }

    return FALSE;
}

void
utils_connect_mouse_hover(GtkWidget* widget)
{
    g_signal_connect(widget, "enter-notify-event", G_CALLBACK(utils_mouse_hover_enter_cb), NULL);
    g_signal_connect(widget, "leave-notify-event", G_CALLBACK(utils_mouse_hover_leave_cb), NULL);
}

void
utils_connect_link(GtkWidget* widget, const gchar* link)
{
    gchar* tmp = g_strdup(link); //TODO: Free this
    utils_connect_mouse_hover(widget);
    g_signal_connect(widget, "button-press-event", G_CALLBACK(utils_mouse_clicked_link_cb), tmp);
}

gboolean
utils_str_empty(const gchar* str)
{
    return !(str && strlen(str) > 0);
}

gchar*
utils_str_capitalise(const gchar* str)
{
    g_assert_false(utils_str_empty(str));

    gchar* ret = g_strdup_printf("%c%s", g_ascii_toupper(*str), str+1);

    return ret;
}

typedef struct
{
    gpointer instance;
    GCallback cb;
    gpointer udata;
} OneshotData;

static void
oneshot_cb(OneshotData* data)
{
    g_signal_handlers_disconnect_by_func(data->instance,
                                         data->cb,
                                         data->udata);
    g_signal_handlers_disconnect_by_func(data->instance,
                                         oneshot_cb,
                                         data);
}

void
utils_signal_connect_oneshot(gpointer instance,
                             const gchar* signal,
                             GCallback cb,
                             gpointer udata)
{
    OneshotData* data = g_new(OneshotData, 1);

    data->instance = instance;
    data->cb = cb;
    data->udata = udata;

    g_signal_connect(instance, signal, cb, udata);

    g_signal_connect_data(instance,
                          signal,
                          G_CALLBACK(oneshot_cb),
                          data,
                          (GClosureNotify) g_free,
                          G_CONNECT_AFTER | G_CONNECT_SWAPPED);
}

void
utils_signal_connect_oneshot_swapped(gpointer instance,
    const gchar* signal, GCallback cb, gpointer udata)
{

    OneshotData* data = g_new(OneshotData, 1);

    data->instance = instance;
    data->cb = cb;
    data->udata = udata;

    g_signal_connect_swapped(instance, signal, cb, udata);

    g_signal_connect_data(instance,
                          signal,
                          G_CALLBACK(oneshot_cb),
                          data,
                          (GClosureNotify) g_free,
                          G_CONNECT_AFTER | G_CONNECT_SWAPPED);
}

inline void
utils_refresh_cancellable(GCancellable** cancel)
{
    if (*cancel && !g_cancellable_is_cancelled(*cancel))
        g_cancellable_cancel(*cancel);
    g_clear_object(cancel);
    *cancel = g_cancellable_new();
}

static void
weak_ref_notify_cb(gpointer udata, GObject* obj) /* NOTE: This object is no longer valid, can only use the addres */
{
    RETURN_IF_FAIL(udata != NULL);

    g_weak_ref_set(udata, NULL);
}

GWeakRef*
utils_weak_ref_new(gpointer obj)
{
    GWeakRef* ref = g_malloc(sizeof(GWeakRef));

    g_weak_ref_init(ref, obj);
    g_object_weak_ref(obj, weak_ref_notify_cb, ref);

    return ref;
}

void
utils_weak_ref_free(GWeakRef* ref)
{
    g_autoptr(GObject) obj = g_weak_ref_get(ref);

    if (obj)
        g_object_weak_unref(obj, weak_ref_notify_cb, ref);

    g_weak_ref_clear(ref);
    g_free(ref);
}

GDateTime*
utils_parse_time_iso_8601(const gchar* time, GError** error)
{
    GDateTime* ret = NULL;

    gint year, month, day, hour, min, sec;

    gint scanned = sscanf(time, "%d-%d-%dT%d:%d:%dZ", &year, &month, &day, &hour, &min, &sec);

    if (scanned != 6)
    {
        g_set_error(error, GT_UTILS_ERROR, GT_UTILS_ERROR_PARSING_TIME,
            "Unable to parse time from input '%s'", time);
    }
    else
        ret = g_date_time_new_utc(year, month, day, hour, min, sec);

    return ret;
}

GenericTaskData*
generic_task_data_new()
{
    return g_slice_new0(GenericTaskData);
}

void
generic_task_data_free(GenericTaskData* data)
{
    g_free(data->str_1);
    g_free(data->str_2);
    g_free(data->str_3);

    g_slice_free(GenericTaskData, data);
}

SoupMessage*
utils_create_twitch_request(const gchar* uri)
{
    RETURN_VAL_IF_FAIL(!utils_str_empty(uri), NULL);

    SoupMessage* msg = NULL;

    msg = soup_message_new(SOUP_METHOD_GET, uri);

    soup_message_headers_append(msg->request_headers, "Client-ID", CLIENT_ID);
    soup_message_headers_append(msg->request_headers, "Accept", "application/vnd.twitchtv.v5+json");
    soup_message_headers_append(msg->request_headers, "Accept", "image/*");

    return msg;
}

SoupMessage*
utils_create_twitch_request_v(const gchar* uri, ...)
{
    RETURN_VAL_IF_FAIL(!utils_str_empty(uri), NULL);

    va_list args;
    g_autofree gchar* uri_formatted = NULL;

    va_start(args, uri);
    uri_formatted = g_strdup_vprintf(uri, args);
    va_end(args);

    return utils_create_twitch_request(uri_formatted);
}

JsonReader*
utils_parse_json(const gchar* data, GError** error)
{
    g_autoptr(JsonReader) reader = NULL;
    g_autoptr(JsonParser) parser = NULL;
    g_autoptr(GError) err = NULL;

    parser = json_parser_new();

    json_parser_load_from_data(parser, data, -1, &err);

    if (err)
    {
        WARNING("Unable to parse JSON because: %s", err->message);

        g_propagate_prefixed_error(error, g_steal_pointer(&err), "Unable to parse JSON because: ");
    }
    else
        reader = json_reader_new(json_parser_get_root(parser));

    return g_steal_pointer(&reader);
}

#define RETURN_JSON_ERROR                                       \
    G_STMT_START                                                \
    {                                                           \
        WARNING("Coultn'd parse channel from JSON because: %s", \
            json_reader_get_error(reader)-> message);           \
        g_set_error(error, GT_UTILS_ERROR, GT_UTILS_ERROR_JSON, \
            "Couldn't parse channel from JSON because: %s",     \
            json_reader_get_error(reader)->message);            \
        return NULL;                                            \
    } G_STMT_END

GtChannelData*
utils_parse_channel_from_json(JsonReader* reader, GError** error)
{
    g_autoptr(GtChannelData) data = gt_channel_data_new();

    if (!json_reader_read_member(reader, "_id")) RETURN_JSON_ERROR;
    JsonNode* node = json_reader_get_value(reader);
    if (STRING_EQUALS(json_node_type_name(node), "Integer"))
        data->id = g_strdup_printf("%" G_GINT64_FORMAT, json_reader_get_int_value(reader));
    else if (STRING_EQUALS(json_node_type_name(node), "String"))
        data->id = g_strdup(json_reader_get_string_value(reader));
    else
    {
        g_set_error(error, GT_UTILS_ERROR, GT_UTILS_ERROR_JSON,
            "Unable to parse game from JSON because: '_id' was of incorrect format");
        return NULL;
    }
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "name")) RETURN_JSON_ERROR;
    data->name = g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "display_name")) RETURN_JSON_ERROR;
    data->display_name = json_reader_get_null_value(reader) ?
        NULL : g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "status")) RETURN_JSON_ERROR;
    data->status = json_reader_get_null_value(reader) ?
        NULL : g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "video_banner")) RETURN_JSON_ERROR;
    data->video_banner_url = json_reader_get_null_value(reader) ?
        NULL : g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "logo")) RETURN_JSON_ERROR;
    data->logo_url = json_reader_get_null_value(reader) ?
        NULL : g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "url")) RETURN_JSON_ERROR;
    data->profile_url = g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    data->online = FALSE;

    return g_steal_pointer(&data);
}

#undef RETURN_JSON_ERROR
#define RETURN_JSON_ERROR                                       \
    G_STMT_START                                                \
    {                                                           \
        WARNING("Coultn'd parse stream from JSON because: %s",  \
            json_reader_get_error(reader)-> message);           \
        g_set_error(error, GT_UTILS_ERROR, GT_UTILS_ERROR_JSON, \
            "Couldn't parse stream from JSON because: %s",      \
            json_reader_get_error(reader)->message);            \
        return NULL;                                            \
    } G_STMT_END

GtChannelData*
utils_parse_stream_from_json(JsonReader* reader, GError** error)
{
    g_autoptr(GtChannelData) data = NULL;

    if (!json_reader_read_member(reader, "channel")) RETURN_JSON_ERROR;
    data = utils_parse_channel_from_json(reader, error);
    json_reader_end_member(reader);

    if (*error) return NULL;

    if (!json_reader_read_member(reader, "game")) RETURN_JSON_ERROR;
    data->game = json_reader_get_null_value(reader) ?
        NULL : g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "viewers")) RETURN_JSON_ERROR;
    data->viewers = json_reader_get_null_value(reader) ?
        0 : json_reader_get_int_value(reader);
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "created_at")) RETURN_JSON_ERROR;
    data->stream_started_time = json_reader_get_null_value(reader) ?
        NULL : utils_parse_time_iso_8601(json_reader_get_string_value(reader), error);
    json_reader_end_member(reader);

    if (*error)
    {
        g_prefix_error(error, "Unable to parse stream from JSON because: ");
        return NULL;
    }

    if (!json_reader_read_member(reader, "preview")) RETURN_JSON_ERROR;
    if (!json_reader_read_member(reader, "large")) RETURN_JSON_ERROR;
    data->preview_url = json_reader_get_null_value(reader) ?
        NULL : g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);
    json_reader_end_member(reader);

    data->online = TRUE;

    return g_steal_pointer(&data);
}

#undef RETURN_JSON_ERROR
#define RETURN_JSON_ERROR                                       \
    G_STMT_START                                                \
    {                                                           \
        WARNING("Coultn'd parse game from JSON because: %s",    \
            json_reader_get_error(reader)-> message);           \
        g_set_error(error, GT_UTILS_ERROR, GT_UTILS_ERROR_JSON, \
            "Couldn't parse game from JSON because: %s",        \
            json_reader_get_error(reader)->message);            \
        return NULL;                                            \
    } G_STMT_END

GtGameData*
utils_parse_game_from_json(JsonReader* reader, GError** error)
{
    g_autoptr(GtGameData) data = gt_game_data_new();

    if (!json_reader_read_member(reader, "_id")) RETURN_JSON_ERROR;
    JsonNode* node = json_reader_get_value(reader);
    if (STRING_EQUALS(json_node_type_name(node), "Integer"))
        data->id = g_strdup_printf("%" G_GINT64_FORMAT, json_reader_get_int_value(reader));
    else if (STRING_EQUALS(json_node_type_name(node), "String"))
        data->id = g_strdup(json_reader_get_string_value(reader));
    else
    {
        g_set_error(error, GT_UTILS_ERROR, GT_UTILS_ERROR_JSON,
            "Unable to parse game from JSON because: '_id' was of incorrect format");
        return NULL;
    }
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "name")) RETURN_JSON_ERROR;
    data->name = g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "box")) RETURN_JSON_ERROR;
    if (!json_reader_read_member(reader, "large")) RETURN_JSON_ERROR;
    data->preview_url = g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "logo")) RETURN_JSON_ERROR;
    if (!json_reader_read_member(reader, "large")) RETURN_JSON_ERROR;
    data->logo_url = g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);
    json_reader_end_member(reader);

    return g_steal_pointer(&data);
}

#undef RETURN_JSON_ERROR
#define RETURN_JSON_ERROR                                       \
    G_STMT_START                                                \
    {                                                           \
        WARNING("Couldn't parse VOD from JSON because: %s",     \
            json_reader_get_error(reader)-> message);           \
        g_set_error(error, GT_UTILS_ERROR, GT_UTILS_ERROR_JSON, \
            "Couldn't parse VOD from JSON because: %s",         \
            json_reader_get_error(reader)->message);            \
        return NULL;                                            \
    } G_STMT_END

GtVODData*
utils_parse_vod_from_json(JsonReader* reader, GError** error)
{
    RETURN_VAL_IF_FAIL(JSON_IS_READER(reader), NULL);

    g_autoptr(GtVODData) data = gt_vod_data_new();
    GTimeVal time;
    JsonNode* node = NULL;

    if (!json_reader_read_member(reader, "_id")) RETURN_JSON_ERROR;
    data->id = g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "broadcast_id")) RETURN_JSON_ERROR;
    node = json_reader_get_value(reader);
    if (STRING_EQUALS(json_node_type_name(node), "Integer"))
        data->broadcast_id = g_strdup_printf("%" G_GINT64_FORMAT, json_reader_get_int_value(reader));
    else if (STRING_EQUALS(json_node_type_name(node), "String"))
        data->broadcast_id = g_strdup(json_reader_get_string_value(reader));
    else
    {
        g_set_error(error, GT_UTILS_ERROR, GT_UTILS_ERROR_JSON,
            "Unable to parse vod from JSON because: '_id' was of incorrect format");
        return NULL;
    }
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "created_at")) RETURN_JSON_ERROR;
    if (!g_time_val_from_iso8601(json_reader_get_string_value(reader), &time))
    {
        g_set_error(error, GT_UTILS_ERROR, GT_UTILS_ERROR_PARSING_TIME,
            "Couldn't parse time from string %s", json_reader_get_string_value(reader));
        return NULL;
    }
    data->created_at = g_date_time_new_from_timeval_utc(&time);
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "published_at")) RETURN_JSON_ERROR;
    if (!g_time_val_from_iso8601(json_reader_get_string_value(reader), &time))
    {
        g_set_error(error, GT_UTILS_ERROR, GT_UTILS_ERROR_PARSING_TIME,
            "Couldn't parse time from string %s", json_reader_get_string_value(reader));
        return NULL;
    }
    data->published_at = g_date_time_new_from_timeval_utc(&time);
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "description")) RETURN_JSON_ERROR;
    data->description = json_reader_get_null_value(reader) ? NULL : g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "game")) RETURN_JSON_ERROR;
    data->game = json_reader_get_null_value(reader) ? NULL : g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "language")) RETURN_JSON_ERROR;
    data->language = json_reader_get_null_value(reader) ? NULL : g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "length")) RETURN_JSON_ERROR;
    data->length = json_reader_get_int_value(reader);
    json_reader_end_member(reader);

    /* NOTE: Uncomment when needed */
    /* if (!json_reader_read_member(reader, "channel")) RETURN_JSON_ERROR; */
    /* data->channel = utils_parse_channel_from_json(reader, error); */
    /* if (*error) return NULL; */
    /* json_reader_end_member(reader); */

    if (!json_reader_read_member(reader, "preview")) RETURN_JSON_ERROR;

    if (!json_reader_read_member(reader, "large")) RETURN_JSON_ERROR;
    data->preview.large = g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "medium")) RETURN_JSON_ERROR;
    data->preview.medium = g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "small")) RETURN_JSON_ERROR;
    data->preview.small = g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "template")) RETURN_JSON_ERROR;
    data->preview.template = g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "title")) RETURN_JSON_ERROR;
    data->title = g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "url")) RETURN_JSON_ERROR;
    data->url = g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "views")) RETURN_JSON_ERROR;
    data->views = json_reader_get_int_value(reader);
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "tag_list")) RETURN_JSON_ERROR;
    data->tag_list = g_strdup(json_reader_get_string_value(reader));
    json_reader_end_member(reader);

    return g_steal_pointer(&data);
}

void
gt_playlist_entry_free(GtPlaylistEntry* entry)
{
    g_free(entry->name);
    g_free(entry->resolution);
    g_free(entry->uri);
    g_slice_free(GtPlaylistEntry, entry);
}

void
gt_playlist_entry_list_free(GtPlaylistEntryList* list)
{
    g_list_free_full(list, (GDestroyNotify) gt_playlist_entry_free);
}

GtPlaylistEntryList*
utils_parse_playlist(const gchar* str, GError** error)
{
    g_autoptr(GtPlaylistEntryList) entries = NULL;
    g_auto(GStrv) lines = g_strsplit(str, "\n", 0);

    for (gchar** l = lines; *l != NULL; l++)
    {
        if (strncmp(*l, "#EXT-X-STREAM-INF", 17) == 0)
        {
            g_autoptr(GtPlaylistEntry) entry = g_slice_new0(GtPlaylistEntry);
            g_auto(GStrv) values = NULL;

            if (strncmp(*(l - 1), "#EXT-X-MEDIA", 12) != 0)
            {
                g_set_error(error, GT_UTILS_ERROR, GT_UTILS_ERROR_PARSING_PLAYLIST,
                    "STREAM-INF entry wasn't preceded by a MEDIA entry");
                return NULL;
            }

            values = g_strsplit(*(l - 1), ",", 0);

            for (gchar** m = values; *m != NULL; m++)
            {
                g_auto(GStrv) split = g_strsplit(*m, "=", 0);

                if (STRING_EQUALS(*split, "NAME"))
                {
                    entry->name = g_strdup(*(split + 1));
                    break;
                }
            }

            values = g_strsplit(*l, ",", 0);

            for (gchar** m = values; *m != NULL; m++)
            {
                g_auto(GStrv) split = g_strsplit(*m, "=", 0);

                if (STRING_EQUALS(*split, "RESOLUTION"))
                {
                    entry->resolution = g_strdup(*(split + 1));
                    break;
                }
            }

            if (!(g_str_has_prefix(*(l + 1), "https://") || g_str_has_prefix(*(l + 1), "http://")))
            {
                g_set_error(error, GT_UTILS_ERROR, GT_UTILS_ERROR_PARSING_PLAYLIST,
                    "STREAM-INF entry wasn't succeeded by a uri");
                return NULL;
            }

            entry->uri = g_strdup(*(l + 1));

            entries = g_list_append(entries, g_steal_pointer(&entry));
        }
    }

    return g_steal_pointer(&entries);
}
