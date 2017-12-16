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

#include "gt-twitch-channel-info-dlg.h"
#include "gt-twitch.h"
#include "gt-app.h"
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <webkit2/webkit2.h>

#define TAG "GtTwitchChannelInfoDlg"
#include "utils.h"

#define HTML_PARAGRAPH "p"
#define HTML_LIST "ul"
#define HTML_LIST_ITEM "li"
#define HTML_STRONG "strong"
#define HTML_LINK "a"
#define HTML_HEADER_1 "h1"

typedef struct
{
    GtkWidget* panel_box_1;
    GtkWidget* panel_box_2;
    GtkWidget* panel_box_3;

    GMarkupParseContext* ctxt;

    GList* link_tags;

    GtkTextView* cur_view;
    GtkTextBuffer* cur_buff;
    gchar* cur_link;
    GQueue* offset_queue;

    GCancellable* cancel;
} GtTwitchChannelInfoDlgPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtTwitchChannelInfoDlg, gt_twitch_channel_info_dlg, GTK_TYPE_DIALOG)

enum
{
    PROP_0,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtTwitchChannelInfoDlg*
gt_twitch_channel_info_dlg_new(GtkWindow* parent)
{
    return g_object_new(GT_TYPE_TWITCH_CHANNEL_INFO_DLG,
                        "transient-for", parent,
                        "use-header-bar", 1,
                        NULL);
}

static gboolean
tag_event_cb(GtkTextTag* tag,
             GObject* obj,
             GdkEvent* evt,
             GtkTextIter* iter,
             gpointer udata)
{
    GtTwitchChannelInfoDlg* self = GT_TWITCH_CHANNEL_INFO_DLG(udata);

    if (evt->type == GDK_BUTTON_PRESS)
    {
        if (((GdkEventButton*) evt)->button == 1)
        {
            /* FIXME: Fix this once we re-enable this feature */
            /* gtk_show_uri(gtk_widget_get_screen(GTK_WIDGET(self)), */
            /*              g_object_get_data(G_OBJECT(tag), "link"), */
            /*              GDK_CURRENT_TIME, NULL); */
        }
    }

    return FALSE;
}

static gboolean
text_panel_motion_cb(GtkWidget* widget,
                     GdkEventMotion* evt,
                     gpointer udata)
{
    GtTwitchChannelInfoDlg* self = GT_TWITCH_CHANNEL_INFO_DLG(udata);
    GtTwitchChannelInfoDlgPrivate* priv = gt_twitch_channel_info_dlg_get_instance_private(self);
    gint buf_x, buf_y;
    GtkTextIter iter;
    GdkCursor* cursor = NULL;

    gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(widget),
                                          GTK_TEXT_WINDOW_WIDGET,
                                          evt->x, evt->y,
                                          &buf_x, &buf_y);

    gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(widget), &iter, buf_x, buf_y);

    for (GList* l = priv->link_tags; l != NULL; l = l->next)
    {
        GtkTextTag* tag = GTK_TEXT_TAG(l->data);

        if (gtk_text_iter_has_tag(&iter, tag))
        {
            cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_HAND2);

            break;
        }
    }

    if (!cursor) cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_XTERM);

    gdk_window_set_cursor(evt->window, cursor);

    g_object_unref(cursor);

    return FALSE;
}

//TODO Parse more tags
static void
start_element(GMarkupParseContext* ctxt,
              const gchar* element_name,
              const gchar** attr_names,
              const gchar** attr_vals,
              gpointer udata,
              GError** error)
{
    GtTwitchChannelInfoDlg* self = GT_TWITCH_CHANNEL_INFO_DLG(udata);
    GtTwitchChannelInfoDlgPrivate* priv = gt_twitch_channel_info_dlg_get_instance_private(self);
    GtkTextIter iter;

    gtk_text_buffer_get_end_iter(priv->cur_buff, &iter);

    if (g_strcmp0(element_name, HTML_PARAGRAPH) == 0)
    {
        //Do nothing
    }
    else if (g_strcmp0(element_name, HTML_LINK) == 0)
    {
        const gchar** n = attr_names;
        const gchar** v = attr_vals;
        for (n = attr_names; n != NULL; n++, v++)
        {
            if (g_strcmp0(*n, "href") == 0)
            {
                gint* pos = g_malloc(sizeof(gint)*1);
                *pos = gtk_text_iter_get_offset(&iter);
                g_queue_push_head(priv->offset_queue, pos);
                priv->cur_link = g_strdup(*v);
                break;
            }
        }
    }
    else if (g_strcmp0(element_name, HTML_STRONG) == 0)
    {
        gint* pos = g_malloc(sizeof(gint)*1);
        *pos = gtk_text_iter_get_offset(&iter);
        g_queue_push_head(priv->offset_queue, pos);

    }
    else if (g_strcmp0(element_name, HTML_LIST) == 0)
    {
        //Do nothing
    }
    else if (g_strcmp0(element_name, HTML_LIST_ITEM) == 0)
    {
        gtk_text_buffer_insert(priv->cur_buff, &iter, "• ", -1);
    }
    else if (g_strcmp0(element_name, HTML_HEADER_1) == 0)
    {
        gint* pos = g_malloc(sizeof(gint)*1);
        *pos = gtk_text_iter_get_offset(&iter);
        g_queue_push_head(priv->offset_queue, pos);
    }
}

static void
end_element(GMarkupParseContext* ctxt,
            const gchar* element_name,
            gpointer udata,
            GError** error)
{
    GtTwitchChannelInfoDlg* self = GT_TWITCH_CHANNEL_INFO_DLG(udata);
    GtTwitchChannelInfoDlgPrivate* priv = gt_twitch_channel_info_dlg_get_instance_private(self);
    GtkTextIter iter;

    gtk_text_buffer_get_end_iter(priv->cur_buff, &iter);

    if (g_strcmp0(element_name, HTML_PARAGRAPH) == 0)
    {
        gtk_text_buffer_insert(priv->cur_buff, &iter, "\n\n", -1);
    }
    else if (g_strcmp0(element_name, HTML_LINK) == 0)
    {
        GtkTextIter prev_iter;
        GtkTextTag* tag;
        gint* offset = g_queue_pop_head(priv->offset_queue);

        gtk_text_buffer_get_iter_at_offset(priv->cur_buff, &prev_iter, *offset);

        tag = gtk_text_buffer_create_tag(priv->cur_buff, NULL,
                                         "underline", PANGO_UNDERLINE_SINGLE,
                                         "foreground", "blue",
                                         NULL);
        g_object_set_data(G_OBJECT(tag), "link", priv->cur_link);
        g_signal_connect(tag, "event", G_CALLBACK(tag_event_cb), self);
        priv->link_tags = g_list_append(priv->link_tags, tag);

        gtk_text_buffer_apply_tag(priv->cur_buff, tag, &prev_iter, &iter);

        g_free(offset);
    }
    else if (g_strcmp0(element_name, HTML_STRONG) == 0)
    {
        GtkTextIter prev_iter;
        gint* offset = g_queue_pop_head(priv->offset_queue);

        gtk_text_buffer_get_iter_at_offset(priv->cur_buff, &prev_iter, *offset);

        gtk_text_buffer_apply_tag_by_name(priv->cur_buff, "bold", &prev_iter, &iter);

        g_free(offset);
    }
    else if (g_strcmp0(element_name, HTML_LIST) == 0)
    {
        //Do nothing
    }
    else if (g_strcmp0(element_name, HTML_LIST_ITEM) == 0)
    {
        gtk_text_buffer_insert(priv->cur_buff, &iter, "\n", -1);
    }
    else if (g_strcmp0(element_name, HTML_HEADER_1) == 0)
    {
        GtkTextIter prev_iter;
        gint* offset = g_queue_pop_head(priv->offset_queue);

        gtk_text_buffer_get_iter_at_offset(priv->cur_buff, &prev_iter, *offset);

        gtk_text_buffer_apply_tag_by_name(priv->cur_buff, "h1", &prev_iter, &iter);

        g_free(offset);

        gtk_text_buffer_insert(priv->cur_buff, &iter, "\n\n", -1);
    }

}

static void
text(GMarkupParseContext* ctxt,
     const gchar* text,
     gsize len,
     gpointer udata,
     GError** error)
{
    GtTwitchChannelInfoDlg* self = GT_TWITCH_CHANNEL_INFO_DLG(udata);
    GtTwitchChannelInfoDlgPrivate* priv = gt_twitch_channel_info_dlg_get_instance_private(self);
    GtkTextIter iter;

    gtk_text_buffer_get_end_iter(priv->cur_buff, &iter);

    gtk_text_buffer_insert(priv->cur_buff, &iter, text, -1);
}

static GMarkupParser parser =
{
    start_element,
    end_element,
    text,
    NULL,
    NULL
};

static void
parse_html_description(GtTwitchChannelInfoDlg* self,  GtkTextView* view, const gchar* text)
{
    GtTwitchChannelInfoDlgPrivate* priv = gt_twitch_channel_info_dlg_get_instance_private(self);

    priv->cur_view = view;
    priv->cur_buff = gtk_text_view_get_buffer(view);

    g_markup_parse_context_parse(priv->ctxt, text, -1, NULL);
}

static GtkWidget*
create_default_panel(GtTwitchChannelInfoDlg* self, GtTwitchChannelInfoPanel* panel)
{
    GtTwitchChannelInfoDlgPrivate* priv = gt_twitch_channel_info_dlg_get_instance_private(self);

    GtkBuilder* builder;
    GtkWidget* panel_box;
    GtkWidget* panel_label;
    GtkWidget* panel_image;
    GtkWidget* panel_text;
    GtkWidget* evt_box;
    GtkTextTag* bold_tag;
    GtkTextTag* link_tag;
    GtkTextTag* h1_tag;
    GtkTextTagTable* tag_table;

    builder = gtk_builder_new_from_resource("/com/vinszent/GnomeTwitch/ui/gt-twitch-channel-info-panel.ui");
    panel_box =  GTK_WIDGET(gtk_builder_get_object(builder, "panel_box"));
    panel_label = GTK_WIDGET(gtk_builder_get_object(builder, "panel_label"));
    panel_image = GTK_WIDGET(gtk_builder_get_object(builder, "panel_image"));
    panel_text = GTK_WIDGET(gtk_builder_get_object(builder, "panel_text"));
    evt_box = GTK_WIDGET(gtk_builder_get_object(builder, "evt_box"));

    if (panel->link && strlen(panel->link) > 0)
    {
        /* utils_connect_link(evt_box, panel->link); */
    }

    g_signal_connect(panel_text, "motion-notify-event", G_CALLBACK(text_panel_motion_cb), self);

    bold_tag = gtk_text_tag_new("bold");
    link_tag = gtk_text_tag_new("link");
    h1_tag = gtk_text_tag_new("h1");

    g_object_set(bold_tag, "weight", PANGO_WEIGHT_BOLD, NULL);
    g_object_set(link_tag, "underline", PANGO_UNDERLINE_SINGLE, NULL);
    g_object_set(h1_tag,
                 "weight", PANGO_WEIGHT_BOLD,
                 "scale", PANGO_SCALE_LARGE,
                 NULL);


    tag_table = gtk_text_buffer_get_tag_table(gtk_text_view_get_buffer(GTK_TEXT_VIEW(panel_text)));
    gtk_text_tag_table_add(tag_table, bold_tag);
    gtk_text_tag_table_add(tag_table, link_tag);
    gtk_text_tag_table_add(tag_table, h1_tag);

    if (panel->title && strlen(panel->title) > 0)
        gtk_label_set_text(GTK_LABEL(panel_label), panel->title);
    else
        gtk_widget_set_visible(panel_label, FALSE);

    if (panel->image)
        gtk_image_set_from_pixbuf(GTK_IMAGE(panel_image), panel->image);
    else
        gtk_widget_set_visible(panel_image, FALSE);

    if (panel->html_description)
        parse_html_description(self, GTK_TEXT_VIEW(panel_text), panel->html_description);
    else
        gtk_widget_set_visible(panel_text, FALSE);

    return panel_box;
}

static void
finalise(GObject* obj)
{
    GtTwitchChannelInfoDlg* self = GT_TWITCH_CHANNEL_INFO_DLG(obj);
    GtTwitchChannelInfoDlgPrivate* priv = gt_twitch_channel_info_dlg_get_instance_private(self);

    g_cancellable_cancel(priv->cancel);
    g_object_unref(priv->cancel);

    G_OBJECT_CLASS(gt_twitch_channel_info_dlg_parent_class)->finalize(obj);
}

static void
get_property(GObject* obj,
             guint prop,
             GValue* val,
             GParamSpec* pspec)
{
    GtTwitchChannelInfoDlg* self = GT_TWITCH_CHANNEL_INFO_DLG(obj);
    GtTwitchChannelInfoDlgPrivate* priv = gt_twitch_channel_info_dlg_get_instance_private(self);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
set_property(GObject* obj,
             guint prop,
             const GValue* val,
             GParamSpec* pspec)
{
    GtTwitchChannelInfoDlg* self = GT_TWITCH_CHANNEL_INFO_DLG(obj);
    GtTwitchChannelInfoDlgPrivate* priv = gt_twitch_channel_info_dlg_get_instance_private(self);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_twitch_channel_info_dlg_class_init(GtTwitchChannelInfoDlgClass* klass)
{
    GObjectClass* obj_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);

    WEBKIT_TYPE_WEB_VIEW;

    obj_class->finalize = finalise;
    obj_class->get_property = get_property;
    obj_class->set_property = set_property;

    gtk_widget_class_set_template_from_resource(widget_class,
                                                "/com/vinszent/GnomeTwitch/ui/gt-twitch-channel-info-dlg.ui");

    gtk_widget_class_bind_template_child_private(widget_class,
                                                 GtTwitchChannelInfoDlg, panel_box_1);
    gtk_widget_class_bind_template_child_private(widget_class,
                                                 GtTwitchChannelInfoDlg, panel_box_2);
    gtk_widget_class_bind_template_child_private(widget_class,
                                                 GtTwitchChannelInfoDlg, panel_box_3);
}

static void
gt_twitch_channel_info_dlg_init(GtTwitchChannelInfoDlg* self)
{
    GtTwitchChannelInfoDlgPrivate* priv = gt_twitch_channel_info_dlg_get_instance_private(self);

    gtk_widget_init_template(GTK_WIDGET(self));

    priv->ctxt = g_markup_parse_context_new(&parser, 0, self, NULL);
    priv->offset_queue = g_queue_new();
    priv->link_tags = NULL;
    priv->cancel = g_cancellable_new();
}

static void
channel_info_cb(GObject* source,
                GAsyncResult* res,
                gpointer udata)
{
    GList* panel_list = NULL;

    if (!(panel_list = g_task_propagate_pointer(G_TASK(res), NULL))) return;

    GtTwitchChannelInfoDlg* self = GT_TWITCH_CHANNEL_INFO_DLG(udata);
    GtTwitchChannelInfoDlgPrivate* priv = gt_twitch_channel_info_dlg_get_instance_private(self);
    gint pos = 0;

    for (GList* l = panel_list; l != NULL; l = l->next, pos++)
    {
        GtTwitchChannelInfoPanel* panel = l->data;
        GtkWidget* widget = create_default_panel(self, panel);
        switch(pos % 3)
        {
            case 0:
                gtk_container_add(GTK_CONTAINER(priv->panel_box_1), widget);
                break;
            case 1:
                gtk_container_add(GTK_CONTAINER(priv->panel_box_2), widget);
                break;
            case 2:
                gtk_container_add(GTK_CONTAINER(priv->panel_box_3), widget);
                break;
        }
    }

    g_list_free_full(panel_list, (GDestroyNotify) gt_twitch_channel_info_panel_free);
}

void
gt_twitch_channel_info_dlg_load_channel(GtTwitchChannelInfoDlg* self, const gchar* chan)
{
    GtTwitchChannelInfoDlgPrivate* priv = gt_twitch_channel_info_dlg_get_instance_private(self);

    gt_twitch_channel_info_async(main_app->twitch, chan, priv->cancel, (GAsyncReadyCallback) channel_info_cb, self); //TODO Use a cancellable
}
