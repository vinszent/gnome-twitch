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

const char* default_chat_colours[] = {"#FF0000", "#0000FF", "#008000", "#B22222", "#FF7F50", "#9ACD32", "#FF4500",
                                      "#2E8B57", "#DAA520", "#D2691E", "#5F9EA0", "#1E90FF", "#FF69B4", "#8A2BE2", "#00FF7F"};

typedef struct
{
    gboolean dark_theme;
    gdouble opacity;
    gchar* cur_theme;

    GtkWidget* chat_view;
    GtkWidget* chat_scroll;
    GtkWidget* chat_entry;
    GtkTextBuffer* chat_buffer;
    GtkTextTagTable* tag_table;
    GHashTable* twitch_emotes;

    GtkTextMark* bottom_mark;
    GtkTextIter bottom_iter;

    GtkCssProvider* chat_css_provider;

    GSimpleActionGroup* action_group;
    GPropertyAction* dark_theme_action;

    GtTwitchChatBadges* chat_badges;

    GRand* rand;
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
pick_random_default_chat_colour(GtTwitchChatView* self)
{
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);

    return default_chat_colours[g_rand_int_range(priv->rand, 0, 12)];
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
             GList* emotes,
             gint user_modes)
{
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);
    GtkTextTag* sender_colour_tag = NULL;
    gint offset = 0;

    gtk_text_buffer_get_end_iter(priv->chat_buffer, &priv->bottom_iter);

    if (!colour || strlen(colour) < 1) //TODO: Set random colour instead of just black
    {
        gchar tag_name[100];

        g_sprintf(tag_name, "chat_colour_%s", sender);
        sender_colour_tag = gtk_text_tag_table_lookup(priv->tag_table, tag_name);

        if (!sender_colour_tag)
        {
            sender_colour_tag = gtk_text_tag_new(tag_name);
            g_object_set(sender_colour_tag,
                         "foreground", pick_random_default_chat_colour(self),
                         "weight", PANGO_WEIGHT_BOLD,
                         NULL);
            gtk_text_tag_table_add(priv->tag_table, sender_colour_tag);
        }
    }
    else
    {
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
//    gtk_text_buffer_insert(priv->chat_buffer, &insert_iter, msg, -1);
    insert_message_with_emotes(priv->chat_buffer, &priv->bottom_iter, emotes, msg, offset);
    gtk_text_iter_forward_to_line_end(&priv->bottom_iter);
    gtk_text_buffer_insert(priv->chat_buffer, &priv->bottom_iter, "\n", -1);
    gtk_text_buffer_move_mark(priv->chat_buffer, priv->bottom_mark, &priv->bottom_iter);
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(priv->chat_view), priv->bottom_mark);
}

static void
send_msg_from_entry(GtTwitchChatView* self)
{
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);
    const gchar* msg;

    msg = gtk_entry_get_text(GTK_ENTRY(priv->chat_entry));

    gt_twitch_chat_client_privmsg(main_app->chat, msg);

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
    }

    gt_twitch_chat_message_free(msg);

    return ret;
}

static void
realise_cb(GtkWidget* widget,
           gpointer udata)
{
    GtTwitchChatView* self = GT_TWITCH_CHAT_VIEW(udata);
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);

    gtk_widget_insert_action_group(GTK_WIDGET(GT_WIN_TOPLEVEL(self)),
                                   "chat-view", G_ACTION_GROUP(priv->action_group));
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

    if (!badges)
        return;

    g_print("Here\n");

    priv->chat_badges = badges;
    g_source_set_callback((GSource*) main_app->chat->source, (GSourceFunc) twitch_chat_source_cb, self, NULL);
}

static void
channel_joined_cb(GtTwitchChatClient* chat,
                  const gchar* channel,
                  gpointer udata)
{
    GtTwitchChatView* self = GT_TWITCH_CHAT_VIEW(udata);
    GtTwitchChatViewPrivate* priv = gt_twitch_chat_view_get_instance_private(self);

    //TODO: Make async
    if (priv->chat_badges)
        gt_twitch_chat_badges_free(priv->chat_badges);
    gt_twitch_chat_badges_async(main_app->twitch, channel+1, NULL, (GAsyncReadyCallback) chat_badges_cb, self);

    gtk_text_buffer_set_text(priv->chat_buffer, "", -1);
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

    priv->rand = g_rand_new();

    g_signal_connect(main_app->chat, "channel-joined", G_CALLBACK(channel_joined_cb), self);
    g_signal_connect(priv->chat_entry, "key-press-event", G_CALLBACK(key_press_cb), self);
    g_signal_connect(self, "realize", G_CALLBACK(realise_cb), self);

    ADD_STYLE_CLASS(self, "gt-twitch-chat-view");
}
