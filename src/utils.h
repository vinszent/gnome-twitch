#ifndef _UTILS_H
#define _UTILS_H

#include <gtk/gtk.h>
#include <libsoup/soup.h>

#define REMOVE_STYLE_CLASS(w, n) gtk_style_context_remove_class(gtk_widget_get_style_context(GTK_WIDGET(w)), n)
#define ADD_STYLE_CLASS(w, n) gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(w)), n)
#define ROUND(x) ((guint) ((x) + 0.5))
#define PRINT_BOOL(b) b ? "true" : "false"
#define STRING_EQUALS(a, b) g_strcmp0(a, b) == 0

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
void utils_refresh_cancellable(GCancellable** cancel);

#endif
