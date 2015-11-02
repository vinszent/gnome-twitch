#include "gt-twitch-chat-view.h"
#include "gt-twitch-chat-client.h"
#include "gt-app.h"
#include "utils.h"
#include <string.h>

typedef struct
{
    GtkWidget* chat_view;
    GtkWidget* chat_scroll;
    GtkWidget* chat_entry;
    GtkTextBuffer* chat_buffer;
    GtkTextTagTable* tag_table;
} GtTwitchChatViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtTwitchChatView, gt_twitch_chat_view, GTK_TYPE_BOX)

enum
{
    PROP_0,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtTwitchChatView*
gt_twitch_chat_view_new()
{
    return g_object_new(GT_TYPE_TWITCH_CHAT_VIEW,
                        NULL);
}

static void
add_chat_msg(GtTwitchChatView* self,
             gchar* sender,
             gchar* colour,
             gchar* msg)
{
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);
    GtkTextTag* sender_colour_tag = NULL;
    gchar* line;
    GtkTextIter insert_iter;

    gtk_text_buffer_get_end_iter(priv->chat_buffer, &insert_iter);

    if (!colour || strlen(colour) < 1) //TODO: Set random colour instead of just black
        colour = "#000000";

    sender_colour_tag = gtk_text_tag_table_lookup(priv->tag_table, colour);

    if (!sender_colour_tag)
    {
        sender_colour_tag = gtk_text_tag_new(colour);
        g_object_set(sender_colour_tag,
                     "foreground", colour,
                     "weight", PANGO_WEIGHT_BOLD,
                     NULL);
        gtk_text_tag_table_add(priv->tag_table, sender_colour_tag);
    }

    gtk_text_buffer_insert_with_tags(priv->chat_buffer, &insert_iter, sender, -1, sender_colour_tag, NULL);
    gtk_text_iter_forward_word_end(&insert_iter);
    gtk_text_buffer_insert(priv->chat_buffer, &insert_iter, ": ", -1);
    gtk_text_iter_forward_chars(&insert_iter, 2);
    gtk_text_buffer_insert(priv->chat_buffer, &insert_iter, msg, -1);
    gtk_text_iter_forward_to_line_end(&insert_iter);
    gtk_text_buffer_insert(priv->chat_buffer, &insert_iter, "\n", -1);
}

static gboolean
twitch_chat_source_cb(GtTwitchChatMessage* msg,
                      gpointer udata)
{
    GtTwitchChatView* self = GT_TWITCH_CHAT_VIEW(udata);
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);
    gboolean ret = G_SOURCE_REMOVE;

    if (!g_source_is_destroyed(g_main_current_source()))
    {
        if (g_strcmp0(msg->command, TWITCH_CHAT_CMD_PRIVMSG) == 0)
        {
            gchar* sender = utils_search_key_value_strv(msg->tags, "display-name");
            if (!sender || strlen(sender) < 1)
                sender = msg->nick;
            gchar* colour = utils_search_key_value_strv(msg->tags, "color");
            gchar* msg_str = msg->params;
            strsep(&msg_str, " :");
            add_chat_msg(self, sender, colour, msg_str+1);
        }

        ret = G_SOURCE_CONTINUE;
    }

    gt_twitch_chat_message_free(msg);

    return ret;
}

static void
channel_joined_cb(GtTwitchChatClient* chat,
                  const gchar* channel,
                  gpointer udata)
{
    GtTwitchChatView* self = GT_TWITCH_CHAT_VIEW(udata);
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);

    gtk_text_buffer_set_text(priv->chat_buffer, "", -1);
    g_source_set_callback((GSource*) main_app->chat->source, (GSourceFunc) twitch_chat_source_cb, self, NULL);
}

static void
finalise(GObject* obj)
{
    GtTwitchChatView* self = GT_TWITCH_CHAT_VIEW(obj);
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);

    G_OBJECT_CLASS(gt_twitch_chat_view_parent_class)->finalize(obj);
}

static void
get_property(GObject* obj,
             guint prop,
             GValue* val,
             GParamSpec* pspec)
{
    GtTwitchChatView* self = GT_TWITCH_CHAT_VIEW(obj);
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);

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
    GtTwitchChatView* self = GT_TWITCH_CHAT_VIEW(obj);
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_twitch_chat_view_class_init(GtTwitchChatViewClass* klass)
{
    GObjectClass* obj_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);

    obj_class->finalize = finalise;
    obj_class->get_property = get_property;
    obj_class->set_property = set_property;

    gtk_widget_class_set_template_from_resource(widget_class,
                                                "/com/gnome-twitch/ui/gt-twitch-chat-view.ui");

    gtk_widget_class_bind_template_child_private(widget_class, GtTwitchChatView, chat_view);
    gtk_widget_class_bind_template_child_private(widget_class, GtTwitchChatView, chat_scroll);
    gtk_widget_class_bind_template_child_private(widget_class, GtTwitchChatView, chat_entry);
}

static void
test_cb(GObject* source,
        gpointer udata)
{
    gdouble value;
    g_object_get(source, "upper", &value, NULL);
    g_object_set(source, "value", value, NULL);
}

static void
gt_twitch_chat_view_init(GtTwitchChatView* self)
{
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);

    gtk_widget_init_template(GTK_WIDGET(self));

    priv->chat_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(priv->chat_view));
    priv->tag_table = gtk_text_buffer_get_tag_table(priv->chat_buffer);

    g_signal_connect(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(priv->chat_scroll)), "changed", G_CALLBACK(test_cb), self);
    g_signal_connect(main_app->chat, "channel-joined", G_CALLBACK(channel_joined_cb), self);
}
