#include "utils.h"
#include <stdlib.h>
#include <string.h>

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

void
utils_pixbuf_scale_simple(GdkPixbuf** pixbuf, gint width, gint height, GdkInterpType interp)
{
    if (!*pixbuf)
        return;

    GdkPixbuf* tmp = gdk_pixbuf_scale_simple(*pixbuf, width, height, interp);
    g_clear_object(pixbuf);
    *pixbuf = tmp;
}

GdkPixbuf*
utils_download_picture(SoupSession* soup, const gchar* url)
{
    SoupMessage* msg;
    GdkPixbuf* ret = NULL;
    GInputStream* input;
    GError* err = NULL;

    msg = soup_message_new("GET", url);
    input = soup_session_send(soup, msg, NULL, &err);

    if (err)
    {
        g_warning("{Utils} Error downloading picture code '%d' message '%s'", err->code, err->message);
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

gchar*
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
