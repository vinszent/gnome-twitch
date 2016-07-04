#ifndef _UTILS_H
#define _UTILS_H

#include <gtk/gtk.h>
#include <libsoup/soup.h>

#define REMOVE_STYLE_CLASS(w, n) gtk_style_context_remove_class(gtk_widget_get_style_context(GTK_WIDGET(w)), n)
#define ADD_STYLE_CLASS(w, n) gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(w)), n)
#define UROUND(x) ((guint) ((x) + 0.5))
#define PRINT_BOOL(b) b ? "true" : "false"

#ifndef TAG
#error Tag not defined
#else
#define LOG(lvl, msg) g_log(NULL, lvl, "{%s} %s", TAG, msg)
#define LOGF(lvl, fmt, ...) g_log(NULL, lvl, "{%s} " fmt, TAG, __VA_ARGS__)
#define WARNING(msg) LOG(G_LOG_LEVEL_WARNING, msg)
#define WARNINGF(fmt, ...) LOGF(G_LOG_LEVEL_WARNING, fmt, __VA_ARGS__)
#define MESSAGE(msg) LOG(G_LOG_LEVEL_MESSAGE, msg)
#define MESSAGEF(fmt, ...) LOG(G_LOG_LEVEL_MESSAGE, fmt, __VA_ARGS__)
#define INFO(msg) LOG(G_LOG_LEVEL_INFO, msg)
#define INFOF(fmt, ...) LOG(G_LOG_LEVEL_INFO, fmt, __VA_ARGS__)
#define DEBUG(msg) LOG(G_LOG_LEVEL_DEBUG, msg)
#define DEBUGF(fmt, ...) LOG(G_LOG_LEVEL_DEBUG, fmt, __VA_ARGS__)
#endif

gpointer utils_value_ref_sink_object(const GValue* val);
gchar* utils_value_dup_string_allow_null(const GValue* val);
void utils_container_clear(GtkContainer* cont);
gint64 utils_timestamp_file(const gchar* filename);
gint64 utils_timestamp_now(void);
void utils_pixbuf_scale_simple(GdkPixbuf** pixbuf, gint width, gint height, GdkInterpType interp);
GdkPixbuf* utils_download_picture(SoupSession* soup, const gchar* url);
GdkPixbuf* utils_download_picture_if_newer(SoupSession* soup, const gchar* url, gint64 timestamp);
const gchar* utils_search_key_value_strv(gchar** strv, const gchar* key);
void utils_connect_mouse_hover(GtkWidget* widget);
void utils_connect_link(GtkWidget* widget, const gchar* link);
gboolean utils_str_empty(const gchar* str);
void utils_signal_connect_oneshot(gpointer instance, const gchar* signal, GCallback cb, gpointer udata);

#endif
