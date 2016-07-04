#include "gt-chat.h"
#include "gt-irc.h"
#include "gt-app.h"
#include "gt-win.h"
#include <string.h>
#include <stdlib.h>
#include <glib/gprintf.h>
#include <locale.h>

#define TAG "GtChat"
#include "utils.h"
//TODO: Add emoticon popover
//TODO: Replace copied text pixbufs with emoticon text

#define CHAT_DARK_THEME_CSS_CLASS "dark-theme"
#define CHAT_LIGHT_THEME_CSS_CLASS "light-theme"
#define CHAT_DARK_THEME_CSS ".gt-chat { background-color: rgba(25, 25, 31, %.2f); }"
#define CHAT_LIGHT_THEME_CSS ".gt-chat { background-color: rgba(242, 242, 242, %.2f); }"

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
    GtkWidget* chat_scroll_vbar;
    GtkWidget* chat_entry;
    GtkTextBuffer* chat_buffer;
    GtkTextTagTable* tag_table;
    GHashTable* twitch_emotes;
    GtkWidget* main_stack;
    GtkWidget* connecting_revealer;

    GtkTextMark* bottom_mark;
    GtkTextIter bottom_iter;

    GtkCssProvider* chat_css_provider;

    GtChatBadges* chat_badges;
    GCancellable* chat_badges_cancel;

    GtIrc* chat;
    gchar* cur_chan;

    gboolean chat_sticky;

    GRegex* url_regex;
} GtChatPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtChat, gt_chat, GTK_TYPE_BOX)

enum
{
    PROP_0,
    PROP_DARK_THEME,
    PROP_OPACITY,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtChat*
gt_chat_new()
{
    return g_object_new(GT_TYPE_CHAT,
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

static void
send_msg_from_entry(GtChat* self)
{
    GtChatPrivate* priv = gt_chat_get_instance_private(self);
    const gchar* msg;

    msg = gtk_entry_get_text(GTK_ENTRY(priv->chat_entry));

    gt_irc_privmsg(priv->chat, msg);

    gtk_entry_set_text(GTK_ENTRY(priv->chat_entry), "");
}

static gboolean
irc_source_cb(GtIrcMessage* msg,
              gpointer udata)
{
    GtChat* self = GT_CHAT(udata);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);
    gboolean ret = G_SOURCE_REMOVE;
    GtkTextIter iter;

    gtk_text_buffer_get_end_iter(priv->chat_buffer, &iter);

    if (msg->cmd_type == GT_IRC_COMMAND_PRIVMSG)
    {
        GtIrcCommandPrivmsg* privmsg = msg->cmd.privmsg;
        GtkTextTag* colour_tag;
        const gchar* sender;

        if (!(sender = privmsg->display_name) || strlen(sender) < 1) sender = msg->nick;

        if (!privmsg->colour || strlen(privmsg->colour) < 1)
            privmsg->colour = g_strdup(get_default_chat_colour(msg->nick));

        colour_tag = gtk_text_tag_table_lookup(priv->tag_table, privmsg->colour);

        if (!colour_tag)
        {
            colour_tag = gtk_text_buffer_create_tag(priv->chat_buffer, privmsg->colour,
                                                    "foreground", privmsg->colour,
                                                    "weight", PANGO_WEIGHT_BOLD,
                                                    NULL);
        }

        // Insert user mode pixbufs
#define INSERT_USER_MODE_PIXBUF(mode, name)                             \
        if (privmsg->user_modes & mode)                                 \
        {                                                               \
            gtk_text_buffer_insert_pixbuf(priv->chat_buffer, &iter, priv->chat_badges->name); \
            gtk_text_buffer_insert(priv->chat_buffer, &iter, " ", -1);  \
        }

        INSERT_USER_MODE_PIXBUF(IRC_USER_MODE_SUBSCRIBER, subscriber);
        INSERT_USER_MODE_PIXBUF(IRC_USER_MODE_TURBO, turbo);
        INSERT_USER_MODE_PIXBUF(IRC_USER_MODE_GLOBAL_MOD, global_mod);
        INSERT_USER_MODE_PIXBUF(IRC_USER_MODE_BROADCASTER, broadcaster);
        INSERT_USER_MODE_PIXBUF(IRC_USER_MODE_STAFF, staff);
        INSERT_USER_MODE_PIXBUF(IRC_USER_MODE_ADMIN, admin);
        INSERT_USER_MODE_PIXBUF(IRC_USER_MODE_MOD, mod);

#undef INSERT_USER_MOD_PIXBUF

        gtk_text_buffer_insert_with_tags(priv->chat_buffer, &iter, sender, -1, colour_tag, NULL);
        gtk_text_buffer_insert(priv->chat_buffer, &iter, ": ", -1);

        GMatchInfo* match_info = NULL;
        glong match_offset_start = -1;
        glong match_offset_end = -1;
        GtkTextTag* url_tag = NULL;
        gboolean done_matching = FALSE;

#define UPDATE_URL_OFFSETS_AND_TAG()                                    \
        if (!done_matching)                                             \
        {                                                               \
            gchar* match_string = g_match_info_fetch(match_info, 0);    \
            gint match_start = -1;                                      \
            gint match_end = -1;                                        \
            g_match_info_fetch_pos(match_info, 0, &match_start, &match_end); \
            match_offset_start = g_utf8_pointer_to_offset(privmsg->msg, privmsg->msg + match_start); \
            match_offset_end = g_utf8_pointer_to_offset(privmsg->msg, privmsg->msg + match_end); \
            done_matching = !g_match_info_next(match_info, NULL);       \
            url_tag = gtk_text_buffer_create_tag(priv->chat_buffer, NULL, \
                                                 "foreground", "blue",  \
                                                 "underline", PANGO_UNDERLINE_SINGLE, \
                                                 NULL);                 \
            g_object_set_data(G_OBJECT(url_tag), "url", match_string);  \
        }                                                               \

        if (g_regex_match(priv->url_regex, privmsg->msg, 0, &match_info))
            UPDATE_URL_OFFSETS_AND_TAG();

        gchar* c = g_utf8_offset_to_pointer(privmsg->msg, 0);
        gchar* d = g_utf8_offset_to_pointer(privmsg->msg, 1);
        GList* l = privmsg->emotes;
        GtEmote* emote = NULL;
        for (gint i = 0; i < g_utf8_strlen(privmsg->msg, -1);
             ++i, c = g_utf8_offset_to_pointer(privmsg->msg, i),
                 d = g_utf8_offset_to_pointer(privmsg->msg, i + 1))
        {
            if (l) emote = l->data;

            if (emote && i == emote->start)
            {
                gtk_text_buffer_insert_pixbuf(priv->chat_buffer, &iter, emote->pixbuf);
                l = l->next;
                i = emote->end;
            }
            else if (i >= match_offset_start && i < match_offset_end)
            {
                gtk_text_buffer_insert_with_tags(priv->chat_buffer, &iter, c, (d - c)*sizeof(gchar),
                                                 url_tag, NULL);

                if (i + 1 == match_offset_end)
                    UPDATE_URL_OFFSETS_AND_TAG();
            }
            else
                gtk_text_buffer_insert(priv->chat_buffer, &iter, c, (d - c)*sizeof(gchar));

        }

        g_match_info_free(match_info);

#undef UPDATE_URL_OFFSETS

        gtk_text_buffer_insert(priv->chat_buffer, &iter, "\n", 1);

        gtk_text_buffer_move_mark(priv->chat_buffer, priv->bottom_mark, &iter);

        // Scrolling upwards causes the pos to be further from the bottom than the natural size increment
        if (priv->chat_sticky)
            gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(priv->chat_view), priv->bottom_mark);
    }

    gt_irc_message_free(msg);

    return G_SOURCE_CONTINUE;
}

static void
chat_badges_cb(GObject* source,
               GAsyncResult* res,
               gpointer udata)
{
    GtChat* self = GT_CHAT(udata);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);
    GtChatBadges* badges;

    badges = g_task_propagate_pointer(G_TASK(res), NULL); //TODO: Error handling

    if (priv->chat_badges)
    {
        g_clear_pointer(&priv->chat_badges, (GDestroyNotify) gt_chat_badges_free);
    }

    //TODO: Error handling
    if (!badges)
        return;

    priv->chat_badges = badges;

    gt_irc_connect_and_join_async(priv->chat, priv->cur_chan,
                                  NULL, NULL, NULL);
}

static gboolean
key_press_cb(GtkWidget* widget,
             GdkEventKey* evt,
             gpointer udata)
{
    GtChat* self = GT_CHAT(udata);

    if (evt->keyval == GDK_KEY_Return)
        send_msg_from_entry(self);

    return FALSE;
}

static gboolean
chat_view_button_press_cb(GtkWidget* widget,
                          GdkEventButton* evt,
                          gpointer udata)
{
    GtChat* self = GT_CHAT(udata);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);
    gint x, y;
    GtkTextIter iter;
    GSList* tags;

    gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(priv->chat_view), GTK_TEXT_WINDOW_TEXT,
                                          evt->x, evt->y, &x, &y);
    gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(priv->chat_view), &iter, x, y);
    tags = gtk_text_iter_get_tags(&iter);

    for (GSList* l = tags; l != NULL; l = l->next)
    {
        gchar* url = g_object_get_data(G_OBJECT(l->data), "url");

        if (url)
            gtk_show_uri(gdk_screen_get_default(), url, GDK_CURRENT_TIME, NULL);
    }

    g_slist_free(tags);

    return FALSE;
}

static gboolean
chat_view_motion_cb(GtkWidget* widget,
                    GdkEventMotion* evt,
                    gpointer udata)
{
    GtChat* self = GT_CHAT(udata);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);
    gint x, y;
    GtkTextIter iter;
    GSList* tags = NULL;
    GdkCursor* cursor = NULL;

    gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(widget),
                                          GTK_TEXT_WINDOW_WIDGET,
                                          evt->x, evt->y,
                                          &x, &y);

    gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(widget),
                                       &iter, x, y);

    tags = gtk_text_iter_get_tags(&iter);

    for (GSList* l = tags; l != NULL; l = l->next)
    {
        if (g_object_get_data(G_OBJECT(l->data), "url"))
        {
            cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_HAND2);
            break;
        }
    }

    if (!cursor) cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_XTERM);

    gdk_window_set_cursor(evt->window, cursor);

    g_object_unref(cursor);


    return GDK_EVENT_PROPAGATE;
}

static void
reset_theme_css(GtChat* self)
{
    GtChatPrivate* priv = gt_chat_get_instance_private(self);
    gchar css[200];

    setlocale(LC_NUMERIC, "C");

    g_sprintf(css, priv->dark_theme ? CHAT_DARK_THEME_CSS : CHAT_LIGHT_THEME_CSS,
              priv->opacity);

    setlocale(LC_NUMERIC, ORIGINAL_LOCALE);

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
    GtChat* self = GT_CHAT(udata);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);

    GtChannel* open_chan = NULL;

    g_object_get(GT_WIN_TOPLEVEL(self)->player, "open-channel", &open_chan, NULL);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->main_stack), "chatview");

    if (open_chan)
    {
        gt_chat_connect(self, gt_channel_get_name(open_chan));
        g_object_unref(open_chan);
    }
}

static void
reconnect_cb(GtkButton* button,
             gpointer udata)
{
    GtChat* self = GT_CHAT(udata);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->main_stack), "chatview");

    gt_irc_connect_and_join_async(priv->chat, priv->cur_chan,
                                                 NULL, NULL, NULL);
}

static void
edge_reached_cb(GtkScrolledWindow* scroll,
                GtkPositionType pos,
                gpointer udata)
{
    GtChat* self = GT_CHAT(udata);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);

    if (pos == GTK_POS_BOTTOM)
        priv->chat_sticky = TRUE;
}

static void
anchored_cb(GtkWidget* widget,
            GtkWidget* prev_toplevel,
            gpointer udata)
{
    GtChat* self = GT_CHAT(udata);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);
    GtWin* win = GT_WIN_TOPLEVEL(self);

    g_signal_connect(main_app, "notify::oauth-token", G_CALLBACK(credentials_set_cb), self);
}

static void
error_encountered_cb(GtIrc* client,
                     GError* err,
                     gpointer udata)
{
    GtChat* self = GT_CHAT(udata);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);

    gtk_label_set_label(GTK_LABEL(priv->error_label), err->message);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->main_stack), "errorview");
}

static void
connected_cb(GObject* source,
             GParamSpec* pspec,
             gpointer udata)
{
    GtChat* self = GT_CHAT(udata);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->connecting_revealer),
                                  gt_irc_is_logged_in(priv->chat));

}

static void
after_connected_cb(GObject* source,
                   GParamSpec* pspec,
                   gpointer udata)
{
    GtChat* self = GT_CHAT(udata);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);

    gtk_widget_set_sensitive(priv->chat_entry,
                             gt_app_credentials_valid(main_app));
}

static gboolean
chat_scrolled_cb(GtkWidget* widget,
                 GdkEvent* evt,
                 gpointer udata)
{
    GtChat* self = GT_CHAT(udata);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);

    if (widget == priv->chat_scroll && evt->type == GDK_SCROLL)
    {
        if (((GdkEventScroll*) evt)->delta_y < 0.0)
            priv->chat_sticky = FALSE;
    }
    else if (widget == priv->chat_scroll_vbar && evt->type == GDK_BUTTON_PRESS)
        priv->chat_sticky = FALSE;

    return GDK_EVENT_PROPAGATE;
}

static void
finalise(GObject* obj)
{
    GtChat* self = GT_CHAT(obj);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);

    G_OBJECT_CLASS(gt_chat_parent_class)->finalize(obj);

    g_object_unref(priv->chat);
}

static void
get_property(GObject* obj,
             guint prop,
             GValue* val,
             GParamSpec* pspec)
{
    GtChat* self = GT_CHAT(obj);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);

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
    GtChat* self = GT_CHAT(obj);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);

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
gt_chat_class_init(GtChatClass* klass)
{
    GObjectClass* obj_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);

    obj_class->finalize = finalise;
    obj_class->get_property = get_property;
    obj_class->set_property = set_property;

    gtk_widget_class_set_template_from_resource(widget_class,
                                                "/com/gnome-twitch/ui/gt-chat.ui");

    gtk_widget_class_bind_template_child_private(widget_class, GtChat, chat_view);
    gtk_widget_class_bind_template_child_private(widget_class, GtChat, chat_scroll);
    gtk_widget_class_bind_template_child_private(widget_class, GtChat, chat_entry);
    gtk_widget_class_bind_template_child_private(widget_class, GtChat, main_stack);
    gtk_widget_class_bind_template_child_private(widget_class, GtChat, error_label);
    gtk_widget_class_bind_template_child_private(widget_class, GtChat, connecting_revealer);
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
gt_chat_init(GtChat* self)
{
    GtChatPrivate* priv = gt_chat_get_instance_private(self);

    gtk_widget_init_template(GTK_WIDGET(self));

    priv->chat_css_provider = gtk_css_provider_new();
    gtk_style_context_add_provider(gtk_widget_get_style_context(GTK_WIDGET(self)),
                                   GTK_STYLE_PROVIDER(priv->chat_css_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_USER);

    priv->chat_scroll_vbar = gtk_scrolled_window_get_vscrollbar(GTK_SCROLLED_WINDOW(priv->chat_scroll));
    priv->chat_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(priv->chat_view));
    priv->tag_table = gtk_text_buffer_get_tag_table(priv->chat_buffer);
    priv->twitch_emotes = g_hash_table_new(g_direct_hash, g_direct_equal);
    gtk_text_buffer_get_end_iter(priv->chat_buffer, &priv->bottom_iter);
    priv->bottom_mark = gtk_text_buffer_create_mark(priv->chat_buffer, "end", &priv->bottom_iter, TRUE);

    priv->chat = gt_irc_new();
    priv->cur_chan = NULL;

    priv->joined_channel = FALSE;
    priv->chat_sticky = TRUE;

    priv->url_regex = g_regex_new("(https?://([-\\w\\.]+)+(:\\d+)?(/([\\w/_\\.]*(\\?\\S+)?)?)?)",
                                  G_REGEX_OPTIMIZE, 0, NULL);

    g_signal_connect(priv->chat_entry, "key-press-event", G_CALLBACK(key_press_cb), self);
    utils_signal_connect_oneshot(self, "hierarchy-changed", G_CALLBACK(anchored_cb), self);
    g_signal_connect(priv->chat, "error-encountered", G_CALLBACK(error_encountered_cb), self);
    g_signal_connect(priv->chat, "notify::logged-in", G_CALLBACK(connected_cb), self);
    g_signal_connect_after(priv->chat, "notify::logged-in", G_CALLBACK(after_connected_cb), self);
    g_signal_connect(priv->chat_scroll, "edge-reached", G_CALLBACK(edge_reached_cb), self);
    g_signal_connect(priv->chat_view, "button-press-event", G_CALLBACK(chat_view_button_press_cb), self);
    g_signal_connect(priv->chat_view, "motion-notify-event", G_CALLBACK(chat_view_motion_cb), self);
    g_signal_connect(priv->chat_scroll, "scroll-event", G_CALLBACK(chat_scrolled_cb), self);
    g_signal_connect(priv->chat_scroll_vbar, "button-press-event", G_CALLBACK(chat_scrolled_cb), self);

    g_object_bind_property(priv->chat, "logged-in",
                           priv->connecting_revealer, "reveal-child",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
    g_object_bind_property(priv->chat, "logged-in",
                           priv->chat_entry, "sensitive",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

    g_source_set_callback((GSource*) priv->chat->source, (GSourceFunc) irc_source_cb, self, NULL);

    ADD_STYLE_CLASS(self, "gt-chat");
}

void
gt_chat_connect(GtChat* self, const gchar* chan)
{
    GtChatPrivate* priv = gt_chat_get_instance_private(self);

    priv->joined_channel = TRUE;
    priv->chat_sticky = TRUE;

    g_clear_pointer(&priv->cur_chan, (GDestroyNotify) g_free);
    priv->cur_chan = g_strdup(chan);

    if (priv->chat_badges_cancel)
         g_cancellable_cancel(priv->chat_badges_cancel);
    g_clear_object(&priv->chat_badges_cancel);
    priv->chat_badges_cancel = g_cancellable_new();

    gt_chat_badges_async(main_app->twitch, chan,
                                priv->chat_badges_cancel,
                                (GAsyncReadyCallback) chat_badges_cb, self);
}

void
gt_chat_disconnect(GtChat* self)
{
    GtChatPrivate* priv = gt_chat_get_instance_private(self);

    priv->joined_channel = FALSE;

    if (gt_irc_is_connected(priv->chat))
        gt_irc_disconnect(priv->chat);

    gtk_text_buffer_set_text(priv->chat_buffer, "", -1);
}
