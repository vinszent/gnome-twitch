#ifndef _UTILS_H
#define _UTILS_H

#include <gtk/gtk.h>

gpointer g_value_ref_sink_object(const GValue* val);
void gtk_container_clear(GtkContainer* cont);
gchar* g_value_dup_string_allow_null(const GValue* val);
gchar* strrpl(gchar *str, gchar *orig, gchar *rep);
void utils_pixbuf_scale_simple(GdkPixbuf** pixbuf, gint width, gint height, GdkInterpType interp);

#endif

