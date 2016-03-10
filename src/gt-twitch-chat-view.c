#include "gt-twitch-chat-view.h"
#include "gt-twitch-chat-client.h"
#include "gt-app.h"
#include "gt-win.h"
#include "utils.h"
#include <string.h>
#include <stdlib.h>
#include <glib/gprintf.h>

#define CHAT_DARK_THEME_CSS_CLASS "dark-theme"
#define CHAT_LIGHT_THEME_CSS_CLASS "light-theme"
#define CHAT_DARK_THEME_CSS ".gt-twitch-chat-view { background-color: rgba(25, 25, 31, %.2f); }"
#define CHAT_LIGHT_THEME_CSS ".gt-twitch-chat-view { background-color: rgba(242, 242, 242, %.2f); }"

#define USER_MODE_GLOBAL_MOD 1
#define USER_MODE_ADMIN 1 << 2
#define USER_MODE_BROADCASTER 1 << 3
#define USER_MODE_MOD 1 << 4
#define USER_MODE_STAFF 1 << 5
#define USER_MODE_TURBO 1 << 6
#define USER_MODE_SUBSCRIBER 1 << 7

const char* default_chat_colours[] = {"#FF0000", "#0000FF", "#00FF00", "#B22222", "#FF7F50", "#9ACD32", "#FF4500",
                                      "#2E8B57", "#DAA520", "#D2691E", "#5F9EA0", "#1E90FF", "#FF69B4", "#8A2BE2", "#00FF7F"};

typedef struct
{
    gboolean dark_theme;
    gdouble opacity;
    gchar* cur_theme;

    gboolean joined_channel;

    GtkWidget* error_label;
    GtkWidget* chat_view;
    GtkWidget* chat_scroll;
    GtkAdjustment* chat_adjustment;
    GtkWidget* chat_entry;
    GtkTextBuffer* chat_buffer;
    GtkTextTagTable* tag_table;
    GHashTable* twitch_emotes;
    GtkWidget* main_stack;
    GtkWidget* connecting_revealer;

    GtkTextMark* bottom_mark;
    GtkTextIter bottom_iter;

    GtkCssProvider* chat_css_provider;

    GSimpleActionGroup* action_group;
    GPropertyAction* dark_theme_action;

    GtTwitchChatBadges* chat_badges;
    GCancellable* chat_badges_cancel;

    GtTwitchChatClient* chat;
    gchar* cur_chan;

    gdouble prev_scroll_val;
    gdouble prev_scroll_upper;
    gboolean prev_sticky;

    gboolean chat_sticky;

} GtTwitchChatViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtTwitchChatView, gt_twitch_chat_view, GTK_TYPE_BOX)

enum
{
    PROP_0,
    PROP_DARK_THEME,
    PROP_OPACITY,
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

//TODO: Use "unique" hash
const gchar*
get_default_chat_colour(const gchar* name)
{
    gint total = 0;

    for (const char* c = name; c[0] != '\0'; c++)
        total += c[0];

    return default_chat_colours[total % 13];
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
    gchar* _tmp;
    gchar* emote;

    _tmp = tmp = g_strdup(emotes);

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

    g_free(_tmp);
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
             GList* emotes,
             gint user_modes)
{
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);
    GtkTextTag* sender_colour_tag = NULL;
    gint offset = 0;

    gtk_text_buffer_get_end_iter(priv->chat_buffer, &priv->bottom_iter);

    if (!colour || strlen(colour) < 1) //TODO: Set random colour instead of just black
        colour = get_default_chat_colour(sender);

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

    offset = strlen(sender) + 2;

    //TODO: Cleanup?
#define INSERT_USER_MOD_PIXBUF(mode, name)                              \
    if (user_modes & mode)                                              \
    {                                                                   \
        gtk_text_buffer_insert_pixbuf(priv->chat_buffer, &priv->bottom_iter, priv->chat_badges->name); \
        gtk_text_buffer_insert(priv->chat_buffer, &priv->bottom_iter, " ", -1); \
        gtk_text_iter_forward_chars(&priv->bottom_iter, 2);             \
        offset += 2;                                                    \
    }                                                                   \

    INSERT_USER_MOD_PIXBUF(USER_MODE_SUBSCRIBER, subscriber);
    INSERT_USER_MOD_PIXBUF(USER_MODE_TURBO, turbo);
    INSERT_USER_MOD_PIXBUF(USER_MODE_GLOBAL_MOD, global_mod);
    INSERT_USER_MOD_PIXBUF(USER_MODE_BROADCASTER, broadcaster);
    INSERT_USER_MOD_PIXBUF(USER_MODE_STAFF, staff);
    INSERT_USER_MOD_PIXBUF(USER_MODE_ADMIN, admin);
    INSERT_USER_MOD_PIXBUF(USER_MODE_MOD, mod);

#undef INSERT_USER_MOD_PIXBUF

    gtk_text_buffer_insert_with_tags(priv->chat_buffer, &priv->bottom_iter, sender, -1, sender_colour_tag, NULL);
    gtk_text_iter_forward_word_end(&priv->bottom_iter);
    gtk_text_buffer_insert(priv->chat_buffer, &priv->bottom_iter, ": ", -1);
    gtk_text_iter_forward_chars(&priv->bottom_iter, 2);
    insert_message_with_emotes(priv->chat_buffer, &priv->bottom_iter, emotes, msg, offset);
    gtk_text_iter_forward_to_line_end(&priv->bottom_iter);
    gtk_text_buffer_insert(priv->chat_buffer, &priv->bottom_iter, "\n", -1);
    gtk_text_buffer_move_mark(priv->chat_buffer, priv->bottom_mark, &priv->bottom_iter);

    gdouble cur_val = gtk_adjustment_get_value(priv->chat_adjustment);
    gdouble cur_upper = gtk_adjustment_get_upper(priv->chat_adjustment);

    // Scrolling upwards causes the pos to be further from the bottom than the natural size increment
    if (priv->chat_sticky && cur_val > priv->prev_scroll_val - cur_upper + priv->prev_scroll_upper - 10)
    {
        gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(priv->chat_view), priv->bottom_mark);
    }
    else
        priv->chat_sticky = FALSE;

    priv->prev_scroll_val = cur_val;
    priv->prev_scroll_upper = cur_upper;
}

static void
send_msg_from_entry(GtTwitchChatView* self)
{
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);
    const gchar* msg;

    msg = gtk_entry_get_text(GTK_ENTRY(priv->chat_entry));

    gt_twitch_chat_client_privmsg(priv->chat, msg);

    gtk_entry_set_text(GTK_ENTRY(priv->chat_entry), "");
}

static gboolean
twitch_chat_source_cb(GtTwitchChatMessage* msg,
                      gpointer udata)
{
    GtTwitchChatView* self = GT_TWITCH_CHAT_VIEW(udata);
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);
    gboolean ret = G_SOURCE_REMOVE;

    if (g_strcmp0(msg->command, TWITCH_CHAT_CMD_PRIVMSG) == 0)
    {
        gint user_modes;
        gchar* sender;
        gchar* colour;
        gchar* msg_str;
        gboolean subscriber;
        gboolean turbo;
        gchar* user_type;
        GList* emotes;

        if (g_strcmp0(msg->nick, "twitchnotify") == 0) //TODO: Handle this better
            goto cont;

        user_modes = 0;
        sender = utils_search_key_value_strv(msg->tags, "display-name");
        msg_str = msg->params;
        emotes = parse_emote_string(self, utils_search_key_value_strv(msg->tags, "emotes"));
        subscriber = atoi(utils_search_key_value_strv(msg->tags, "subscriber"));
        turbo = atoi(utils_search_key_value_strv(msg->tags, "turbo"));
        user_type = utils_search_key_value_strv(msg->tags, "user-type");
        colour = utils_search_key_value_strv(msg->tags, "color");

        if (!sender || strlen(sender) < 1)
            sender = msg->nick;

        if (subscriber) user_modes |= USER_MODE_SUBSCRIBER;
        if (turbo) user_modes |= USER_MODE_TURBO;
        if (g_strcmp0(user_type, "mod") == 0) user_modes |= USER_MODE_MOD;
        else if (g_strcmp0(user_type, "global_mod") == 0) user_modes |= USER_MODE_GLOBAL_MOD;
        else if (g_strcmp0(user_type, "admin") == 0) user_modes |= USER_MODE_ADMIN;
        else if (g_strcmp0(user_type, "staff") == 0) user_modes |= USER_MODE_STAFF;

        strsep(&msg_str, " :");
        msg_str++;
        if (msg_str[0] == '\001')
        {
            strsep(&msg_str, " ");
            msg_str[strlen(msg_str) - 1] = '\0';
        }

        add_chat_msg(self, sender, colour, msg_str, emotes, user_modes);
        g_list_free_full(emotes, g_free);
    }

cont:
    ret = G_SOURCE_CONTINUE;

    gt_twitch_chat_message_free(msg);

    return ret;
}

static void
do_nothing()
{
}

static void
chat_badges_cb(GObject* source,
               GAsyncResult* res,
               gpointer udata)
{
    GtTwitchChatView* self = GT_TWITCH_CHAT_VIEW(udata);
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);
    GtTwitchChatBadges* badges;

    badges = g_task_propagate_pointer(G_TASK(res), NULL); //TODO: Error handling

    if (priv->chat_badges)
    {
        gt_twitch_chat_badges_free(priv->chat_badges);
        priv->chat_badges = NULL;
    }

    if (!badges)
        return;

    priv->chat_badges = badges;

//    if (gt_twitch_chat_client_is_connected(priv->chat))
//    gtk_stack_set_visible_child_name(GTK_STACK(priv->main_stack), "chatview");
//    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->connecting_revealer), FALSE);

    gt_twitch_chat_client_connect_and_join_async(priv->chat, priv->cur_chan, NULL, (GAsyncReadyCallback) do_nothing, NULL);

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
reset_theme_css(GtTwitchChatView* self)
{
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);
    gchar css[200];

    g_sprintf(css, priv->dark_theme ? CHAT_DARK_THEME_CSS : CHAT_LIGHT_THEME_CSS,
              priv->opacity);

    if (priv->dark_theme)
    {
        REMOVE_STYLE_CLASS(priv->chat_view, CHAT_LIGHT_THEME_CSS_CLASS);
        REMOVE_STYLE_CLASS(priv->chat_entry, CHAT_LIGHT_THEME_CSS_CLASS);
        ADD_STYLE_CLASS(priv->chat_view, CHAT_DARK_THEME_CSS_CLASS);
        ADD_STYLE_CLASS(priv->chat_entry, CHAT_DARK_THEME_CSS_CLASS);
    }
    else
    {
        REMOVE_STYLE_CLASS(priv->chat_view, CHAT_DARK_THEME_CSS_CLASS);
        REMOVE_STYLE_CLASS(priv->chat_entry, CHAT_DARK_THEME_CSS_CLASS);
        ADD_STYLE_CLASS(priv->chat_view, CHAT_LIGHT_THEME_CSS_CLASS);
        ADD_STYLE_CLASS(priv->chat_entry, CHAT_LIGHT_THEME_CSS_CLASS);
    }

    gtk_css_provider_load_from_data(priv->chat_css_provider, css, -1, NULL); //TODO Error handling
    gtk_widget_reset_style(GTK_WIDGET(self));
    gtk_widget_reset_style(priv->chat_view);
}

static void
credentials_set_cb(GObject* source,
                   GParamSpec* pspec,
                   gpointer udata)
{
    GtTwitchChatView* self = GT_TWITCH_CHAT_VIEW(udata);
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);
    gchar* user_name;
    gchar* oauth_token;

    g_object_get(main_app,
                 "user-name", &user_name,
                 "oauth-token", &oauth_token,
                 NULL);

    if (!user_name || !oauth_token ||
        strlen(user_name) < 1 || strlen(oauth_token) < 1)
    {
        gt_twitch_chat_client_disconnect(priv->chat);
        gtk_text_buffer_set_text(priv->chat_buffer, "", -1);

        gtk_stack_set_visible_child_name(GTK_STACK(priv->main_stack), "loginview");

        priv->joined_channel = FALSE;
    }
    else
    {
        GtChannel* open_chan = NULL;

        g_object_get(GT_WIN_TOPLEVEL(self)->player, "open-channel", &open_chan, NULL);

        gtk_stack_set_visible_child_name(GTK_STACK(priv->main_stack), "chatview");

        if (open_chan && !gt_twitch_chat_client_is_connected(priv->chat))
        {
            gt_twitch_chat_client_connect_and_join(priv->chat, gt_channel_get_name(open_chan));
            g_object_unref(open_chan);
        }
    }

    g_free(user_name);
    g_free(oauth_token);
}

static void
reconnect_cb(GtkButton* button,
             gpointer udata)
{
    GtTwitchChatView* self = GT_TWITCH_CHAT_VIEW(udata);
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->main_stack), "chatview");

    gt_twitch_chat_client_connect_and_join(priv->chat, priv->cur_chan);
}

static void
edge_reached_cb(GtkScrolledWindow* scroll,
                GtkPositionType pos,
                gpointer udata)
{
    GtTwitchChatView* self = GT_TWITCH_CHAT_VIEW(udata);
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);

    if (pos == GTK_POS_BOTTOM)
        priv->chat_sticky = TRUE;
}

static void
anchored_cb(GtkWidget* widget,
            GtkWidget* prev_toplevel,
            gpointer udata)
{
    GtTwitchChatView* self = GT_TWITCH_CHAT_VIEW(udata);
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);
    GtWin* win = GT_WIN_TOPLEVEL(self);

    gtk_widget_insert_action_group(GTK_WIDGET(win),
                                   "chat-view", G_ACTION_GROUP(priv->action_group));

    g_signal_connect(main_app, "notify::oauth-token", G_CALLBACK(credentials_set_cb), self);

    g_signal_handlers_disconnect_by_func(self, anchored_cb, udata); //One-shot
}

static void
error_encountered_cb(GtTwitchChatClient* client,
                     GError* err,
                     gpointer udata)
{
    GtTwitchChatView* self = GT_TWITCH_CHAT_VIEW(udata);
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);

    gtk_label_set_label(GTK_LABEL(priv->error_label), err->message);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->main_stack), "errorview");
}

static void
connected_cb(GObject* source,
             GParamSpec* pspec,
             gpointer udata)
{
    GtTwitchChatView* self = GT_TWITCH_CHAT_VIEW(udata);
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);

    if (gt_twitch_chat_client_is_logged_in(priv->chat))
            gtk_revealer_set_reveal_child(GTK_REVEALER(priv->connecting_revealer), FALSE);
//        gtk_stack_set_visible_child_name(GTK_STACK(priv->main_stack), "chatview");
    else
            gtk_revealer_set_reveal_child(GTK_REVEALER(priv->connecting_revealer), TRUE);
//        gtk_stack_set_visible_child_name(GTK_STACK(priv->main_stack), "connectingview");
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
        case PROP_OPACITY:
            g_value_set_double(val, priv->opacity);
            break;
        case PROP_DARK_THEME:
            g_value_set_boolean(val, priv->dark_theme);
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
        case PROP_OPACITY:
            priv->opacity = g_value_get_double(val);
            reset_theme_css(self);
            break;
        case PROP_DARK_THEME:
            priv->dark_theme = g_value_get_boolean(val);
            reset_theme_css(self);
            break;
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
    gtk_widget_class_bind_template_child_private(widget_class, GtTwitchChatView, main_stack);
    gtk_widget_class_bind_template_child_private(widget_class, GtTwitchChatView, error_label);
    gtk_widget_class_bind_template_child_private(widget_class, GtTwitchChatView, connecting_revealer);
    gtk_widget_class_bind_template_callback(widget_class, reconnect_cb);

    props[PROP_DARK_THEME] = g_param_spec_boolean("dark-theme",
                                                  "Dark Theme",
                                                  "Whether dark theme",
                                                  TRUE,
                                                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    props[PROP_OPACITY] = g_param_spec_double("opacity",
                                              "Opacity",
                                              "Current opacity",
                                              0, 1.0, 1.0,
                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    g_object_class_install_properties(obj_class, NUM_PROPS, props);
}

static void
gt_twitch_chat_view_init(GtTwitchChatView* self)
{
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);

    gtk_widget_init_template(GTK_WIDGET(self));

    priv->chat_adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(priv->chat_scroll));

    priv->action_group = g_simple_action_group_new();
    priv->dark_theme_action = g_property_action_new("dark-theme", self, "dark-theme");
    g_action_map_add_action(G_ACTION_MAP(priv->action_group), G_ACTION(priv->dark_theme_action));

    priv->chat_css_provider = gtk_css_provider_new();
    gtk_style_context_add_provider(gtk_widget_get_style_context(GTK_WIDGET(self)),
                                   GTK_STYLE_PROVIDER(priv->chat_css_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_USER);

    priv->chat_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(priv->chat_view));
    priv->tag_table = gtk_text_buffer_get_tag_table(priv->chat_buffer);
    priv->twitch_emotes = g_hash_table_new(g_direct_hash, g_direct_equal);
    gtk_text_buffer_get_end_iter(priv->chat_buffer, &priv->bottom_iter);
    priv->bottom_mark = gtk_text_buffer_create_mark(priv->chat_buffer, "end", &priv->bottom_iter, TRUE);

    priv->chat = gt_twitch_chat_client_new();
    priv->cur_chan = NULL;

    priv->joined_channel = FALSE;
    priv->chat_sticky = TRUE;
    priv->prev_scroll_val = 0;
    priv->prev_scroll_upper = 0;

    g_signal_connect(priv->chat_entry, "key-press-event", G_CALLBACK(key_press_cb), self);
    g_signal_connect(self, "hierarchy-changed", G_CALLBACK(anchored_cb), self);
    g_signal_connect(priv->chat, "error-encountered", G_CALLBACK(error_encountered_cb), self);
    g_signal_connect(priv->chat, "notify::logged-in", G_CALLBACK(connected_cb), self);
    g_signal_connect(priv->chat_scroll, "edge-reached", G_CALLBACK(edge_reached_cb), self);

    g_object_bind_property(priv->chat, "logged-in",
                           priv->connecting_revealer, "reveal-child",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
    g_object_bind_property(priv->chat, "logged-in",
                           priv->chat_entry, "sensitive",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

    g_source_set_callback((GSource*) priv->chat->source, (GSourceFunc) twitch_chat_source_cb, self, NULL);

    ADD_STYLE_CLASS(self, "gt-twitch-chat-view");
}

void
gt_twitch_chat_view_connect(GtTwitchChatView* self, const gchar* chan)
{
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);

    priv->joined_channel = TRUE;
    priv->chat_sticky = TRUE;

    g_clear_pointer(&priv->cur_chan, (GDestroyNotify) g_free);
    priv->cur_chan = g_strdup(chan);

    if (priv->chat_badges_cancel)
         g_cancellable_cancel(priv->chat_badges_cancel);
    g_clear_object(&priv->chat_badges_cancel);
    priv->chat_badges_cancel = g_cancellable_new();

    gt_twitch_chat_badges_async(main_app->twitch, chan,
                                priv->chat_badges_cancel,
                                (GAsyncReadyCallback) chat_badges_cb, self);
}

void
gt_twitch_chat_view_disconnect(GtTwitchChatView* self)
{
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);

    priv->joined_channel = FALSE;

    if (gt_twitch_chat_client_is_connected(priv->chat))
        gt_twitch_chat_client_disconnect(priv->chat);

    gtk_text_buffer_set_text(priv->chat_buffer, "", -1);
//    gtk_stack_set_visible_child_name(GTK_STACK(priv->main_stack), "connectingview");
}
