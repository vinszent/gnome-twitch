#ifndef _UTILS_H
#define _UTILS_H

#include <gtk/gtk.h>
#include <libsoup/soup.h>

#define REMOVE_STYLE_CLASS(w, n) gtk_style_context_remove_class(gtk_widget_get_style_context(w), n)
#define ADD_STYLE_CLASS(w, n) gtk_style_context_add_class(gtk_widget_get_style_context(w), n)

gpointer g_value_ref_sink_object(const GValue* val);
void gtk_container_clear(GtkContainer* cont);
gchar* g_value_dup_string_allow_null(const GValue* val);
gchar* strrpl(gchar *str, gchar *orig, gchar *rep);
void utils_pixbuf_scale_simple(GdkPixbuf** pixbuf, gint width, gint height, GdkInterpType interp);
GdkPixbuf* utils_download_picture(SoupSession* soup, const gchar* url);

#endif

