#include <glib/gstdio.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "config.h"

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

gint64
utils_timestamp_file(const gchar* filename)
{
    int ret;
    GStatBuf file_stat;

    ret = g_stat(filename, &file_stat);

    if (ret)
        return 0;

    return file_stat.st_mtim.tv_sec;
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

static gint64
utils_http_full_date_to_timestamp(const char* string)
{
    gint64 ret;
    SoupDate* tmp;

    tmp = soup_date_new_from_string(string);
    ret = soup_date_to_time_t(tmp);
    soup_date_free(tmp);

    return ret;
}

GdkPixbuf*
utils_download_picture(SoupSession* soup, const gchar* url)
{
    SoupMessage* msg;
    GdkPixbuf* ret = NULL;
    GInputStream* input;
    GError* err = NULL;

    msg = soup_message_new("GET", url);
    soup_message_headers_append(msg->request_headers, "Client-ID", CLIENT_ID);
    input = soup_session_send(soup, msg, NULL, &err);

    if (err)
    {
        WARNINGF("Error downloading picture, url=%s, code=%d, message=%s", url, err->code, err->message);
        g_error_free(err);
    }
    else
    {
        ret = gdk_pixbuf_new_from_stream(input, NULL, NULL);

        g_input_stream_close(input, NULL, NULL);
    }

    g_object_unref(msg);

    return ret;
}

GdkPixbuf*
utils_download_picture_if_newer(SoupSession* soup, const gchar* url, gint64 timestamp)
{
    SoupMessage* msg;
    guint soup_status;
    const gchar* last_modified;
    GdkPixbuf* ret;

    msg = soup_message_new(SOUP_METHOD_HEAD, url);
    soup_message_headers_append(msg->request_headers, "Client-ID", CLIENT_ID);
    soup_status = soup_session_send_message(soup, msg);

    if (SOUP_STATUS_IS_SUCCESSFUL(soup_status) &&
        (last_modified = soup_message_headers_get_one(msg->response_headers, "Last-Modified")) != NULL &&
        utils_http_full_date_to_timestamp(last_modified) < timestamp)
    {
        g_info("{Utils} No new content at url '%s'", url);
        ret = NULL;
    }
    else
        ret = utils_download_picture(soup, url);

    g_object_unref(msg);

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
    GdkScreen* screen;

    screen = gtk_widget_get_screen(widget);

    if (evt->button == 1 && evt->type == GDK_BUTTON_PRESS)
    {
        gtk_show_uri(screen, (gchar*) udata, GDK_CURRENT_TIME, NULL);
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

GDateTime*
utils_parse_time_iso_8601(const gchar* time, GError** error)
{
    GDateTime* ret = NULL;

    gint year, month, day, hour, min, sec;

    gint scanned = sscanf(time, "%d-%d-%dT%d:%d:%dZ", &year, &month, &day, &hour, &min, &sec);

    if (scanned != 6)
    {
        g_set_error(error, GT_UTILS_ERROR, GT_UTILS_ERROR_PARSING,
            "Unable to parse time from input '%s'", time);
    }
    else
        ret = g_date_time_new_utc(year, month, day, hour, min, sec);

    return ret;
}
