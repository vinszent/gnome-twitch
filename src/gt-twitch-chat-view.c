#include "gt-twitch-chat-view.h"
#include "gt-twitch-chat-client.h"
#include "gt-app.h"
#include "utils.h"
#include <string.h>
#include <stdlib.h>
#include <glib/gprintf.h>

#define CHAT_VIEW_CSS "GtkTextView { background-color: %s; }"

typedef struct
{
    GtkWidget* chat_view;
    GtkWidget* chat_scroll;
    GtkWidget* chat_entry;
    GtkTextBuffer* chat_buffer;
    GtkTextTagTable* tag_table;
    GHashTable* twitch_emotes;

    GtkCssProvider* css_provider;
    GdkRGBA* background_colour;
} GtTwitchChatViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtTwitchChatView, gt_twitch_chat_view, GTK_TYPE_BOX)

enum
{
    PROP_0,
    PROP_BACKGROUND_COLOUR,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

typedef struct
{
    int start;
    int end;
    GdkPixbuf* pixbuf;
} TwitchEmote;

GtTwitchChatView*
gt_twitch_chat_view_new()
{
    return g_object_new(GT_TYPE_TWITCH_CHAT_VIEW,
                        NULL);
}

gint
twitch_emote_compare(TwitchEmote* a, TwitchEmote* b)
{
    if (a->start < b->start)
        return -1;
    else if (a->start > b->start)
        return 1;
    else
        return 0;
}

static GList*
parse_emote_string(GtTwitchChatView* self, const gchar* emotes)
{
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);
    GList* ret = NULL;
    gchar* tmp;
    gchar* emote;

    tmp = g_strdup(emotes);

    while ((emote = strsep(&tmp, "/")) != NULL)
    {
        gint id;
        gchar* indexes;
        gchar* index;
        GdkPixbuf* pixbuf;
        gchar url[128];

        id = atoi(strsep(&emote, ":"));
        indexes = strsep(&emote, ":");

        if (!g_hash_table_contains(priv->twitch_emotes, GINT_TO_POINTER(id)))
        {
            pixbuf = gt_twitch_download_emote(main_app->twitch, id);
            g_hash_table_insert(priv->twitch_emotes, GINT_TO_POINTER(id), pixbuf);
        }
        else
            pixbuf = g_hash_table_lookup(priv->twitch_emotes, GINT_TO_POINTER(id));

        while ((index = strsep(&indexes, ",")) != NULL)
        {
            TwitchEmote* em = g_new0(TwitchEmote, 1);

            em->start = atoi(strsep(&index, "-"));
            em->end = atoi(strsep(&index, "-"));
            em->pixbuf = pixbuf;

            ret = g_list_append(ret, em);
        }
    }

    g_free(tmp);
    ret = g_list_sort(ret, (GCompareFunc) twitch_emote_compare);

    return ret;
}

static void
insert_message_with_emotes(GtkTextBuffer* buf, GtkTextIter* iter, GList* emotes, const gchar* msg, gint offset)
{
    gint deleted = 0;

    gtk_text_buffer_insert(buf, iter, msg, -1);

    if (!emotes)
        return;

    for (GList* l = emotes; l != NULL; l = l->next)
    {
        TwitchEmote* em = (TwitchEmote*) l->data;
        GtkTextIter iter2 = *iter;

        gtk_text_iter_set_line_offset(iter, em->start + offset - deleted);
        gtk_text_iter_set_line_offset(&iter2, em->end + offset - deleted + 1);
        gtk_text_buffer_delete(buf, iter, &iter2);
        gtk_text_buffer_insert_pixbuf(buf, iter, em->pixbuf);
        deleted += em->end - em->start;
    }
}

static void
add_chat_msg(GtTwitchChatView* self,
             const gchar* sender,
             const gchar* colour,
             const gchar* msg,
             GList* emotes)
{
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);
    GtkTextTag* sender_colour_tag = NULL;
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
//    gtk_text_buffer_insert(priv->chat_buffer, &insert_iter, msg, -1);
    insert_message_with_emotes(priv->chat_buffer, &insert_iter, emotes, msg, strlen(sender) + 2);
    gtk_text_iter_forward_to_line_end(&insert_iter);
    gtk_text_buffer_insert(priv->chat_buffer, &insert_iter, "\n", -1);
}

static void
send_msg_from_entry(GtTwitchChatView* self)
{
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);
    const gchar* msg;

    msg = gtk_entry_get_text(GTK_ENTRY(priv->chat_entry));

    gt_twitch_chat_client_privmsg(main_app->chat, msg);

    add_chat_msg(self, gt_app_get_user_name(main_app), "#F5629B", msg, NULL); //TODO: User emotes

    gtk_entry_set_text(GTK_ENTRY(priv->chat_entry), "");
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
            GList* emotes = parse_emote_string(self, utils_search_key_value_strv(msg->tags, "emotes"));
            strsep(&msg_str, " :");
            add_chat_msg(self, sender, colour, msg_str+1, emotes);
            g_list_free_full(emotes, g_free);
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

static gboolean
key_press_cb(GtkWidget* widget,
             GdkEventKey* evt,
             gpointer udata)
{
    GtTwitchChatView* self = GT_TWITCH_CHAT_VIEW(udata);

    if (evt->keyval == GDK_KEY_Return)
        send_msg_from_entry(self);

    return FALSE;
}

static void
background_colour_set_cb(GObject* source,
                         GParamSpec* pspec,
                         gpointer udata)
{
    GtTwitchChatView* self = GT_TWITCH_CHAT_VIEW(udata);
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);

    gchar css[100];

    g_sprintf(css, CHAT_VIEW_CSS, gdk_rgba_to_string(priv->background_colour));

    gtk_css_provider_load_from_data(priv->css_provider, css, -1, NULL); //TODO Error handling
    gtk_widget_reset_style(priv->chat_view);
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
        case PROP_BACKGROUND_COLOUR:
            g_value_set_boxed(val, priv->background_colour);
            break;
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
        case PROP_BACKGROUND_COLOUR:
        {
            if (priv->background_colour)
                gdk_rgba_free(priv->background_colour);
            priv->background_colour = g_value_dup_boxed(val);
            break;
        }
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

    props[PROP_BACKGROUND_COLOUR] = g_param_spec_boxed("background-colour",
                                                       "Background Colour",
                                                       "Current background colour",
                                                       GDK_TYPE_RGBA,
                                                       G_PARAM_READWRITE);

    g_object_class_install_properties(obj_class, NUM_PROPS, props);
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

    priv->css_provider = gtk_css_provider_new();
    gtk_style_context_add_provider(gtk_widget_get_style_context(priv->chat_view),
                                   GTK_STYLE_PROVIDER(priv->css_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_USER);

        priv->chat_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(priv->chat_view));
    priv->tag_table = gtk_text_buffer_get_tag_table(priv->chat_buffer);
    priv->twitch_emotes = g_hash_table_new(g_direct_hash, g_direct_equal);

    g_signal_connect(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(priv->chat_scroll)), "changed", G_CALLBACK(test_cb), self);
    g_signal_connect(main_app->chat, "channel-joined", G_CALLBACK(channel_joined_cb), self);
    g_signal_connect(priv->chat_entry, "key-press-event", G_CALLBACK(key_press_cb), self);
    g_signal_connect(self, "notify::background-colour", G_CALLBACK(background_colour_set_cb), self);

    ADD_STYLE_CLASS(self, "gt-twitch-chat-view");
}
