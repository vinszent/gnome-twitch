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

#ifndef _UTILS_H
#define _UTILS_H

#include "gt-channel.h"
#include "gt-game.h"
#include "gt-vod.h"
#include <gtk/gtk.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

#define REMOVE_STYLE_CLASS(w, n) gtk_style_context_remove_class(gtk_widget_get_style_context(GTK_WIDGET(w)), n)
#define ADD_STYLE_CLASS(w, n) gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(w)), n)
#define ROUND(x) ((gint) ((x) + 0.5))
#define PRINT_BOOL(b) b ? "true" : "false"
#define STRING_EQUALS(a, b) (g_strcmp0(a, b) == 0)

#define GT_UTILS_ERROR g_quark_from_static_string("gt-utils-error-quark")

typedef enum
{
    GT_UTILS_ERROR_PARSING_PLAYLIST,
    GT_UTILS_ERROR_PARSING_TIME,
    GT_UTILS_ERROR_JSON,
    GT_UTILS_ERROR_SOUP,
} GtUtilsError;

typedef struct
{
    gint64 int_1;
    gint64 int_2;
    gint64 int_3;

    gchar* str_1;
    gchar* str_2;
    gchar* str_3;

    gboolean bool_1;
    gboolean bool_2;
    gboolean bool_3;
} GenericTaskData;

typedef struct
{
    gchar* name;
    gchar* resolution;
    gchar* uri;
} GtPlaylistEntry;

typedef GList GtPlaylistEntryList;

gpointer utils_value_ref_sink_object(const GValue* val);
gchar* utils_value_dup_string_allow_null(const GValue* val);
void utils_container_clear(GtkContainer* cont);
guint64 utils_timestamp_filename(const gchar* filename, GError** error);
guint64 utils_timestamp_file(GFile* file, GError** error);
gint64 utils_timestamp_now(void);
guint64 utils_http_full_date_to_timestamp(const char* string);
void utils_pixbuf_scale_simple(GdkPixbuf** pixbuf, gint width, gint height, GdkInterpType interp);
const gchar* utils_search_key_value_strv(gchar** strv, const gchar* key);
void utils_connect_mouse_hover(GtkWidget* widget);
void utils_connect_link(GtkWidget* widget, const gchar* link);
gboolean utils_str_empty(const gchar* str);
gchar* utils_str_capitalise(const gchar* str);
void utils_signal_connect_oneshot(gpointer instance, const gchar* signal, GCallback cb, gpointer udata);
void utils_signal_connect_oneshot_swapped(gpointer instance, const gchar* signal, GCallback cb, gpointer udata);
void utils_refresh_cancellable(GCancellable** cancel);
GDateTime* utils_parse_time_iso_8601(const gchar* time, GError** error);
GenericTaskData* generic_task_data_new();
void generic_task_data_free(GenericTaskData* data);
GWeakRef* utils_weak_ref_new(gpointer obj);
void utils_weak_ref_free(GWeakRef* ref);
SoupMessage* utils_create_twitch_request(const gchar* uri);
SoupMessage* utils_create_twitch_request_v(const gchar* uri, ...);
JsonReader* utils_parse_json(const gchar* data, GError** error);
GtChannelData* utils_parse_stream_from_json(JsonReader* reader, GError** error);
GtChannelData* utils_parse_channel_from_json(JsonReader* reader, GError** error);
GtGameData* utils_parse_game_from_json(JsonReader* reader, GError** error);
GtVODData* utils_parse_vod_from_json(JsonReader* reader, GError** error);
GtPlaylistEntryList* utils_parse_playlist(const gchar* str, GError** error);
void gt_playlist_entry_free(GtPlaylistEntry* entry);
void gt_playlist_entry_list_free(GtPlaylistEntryList* list);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GWeakRef, utils_weak_ref_free);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtPlaylistEntry, gt_playlist_entry_free);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtPlaylistEntryList, gt_playlist_entry_list_free);

#endif
