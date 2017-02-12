#include "gt-chat.h"
#include "gt-irc.h"
#include "gt-app.h"
#include "gt-win.h"
#include <string.h>
#include <stdlib.h>
#include <glib/gprintf.h>
#include <locale.h>
#include <glib/gi18n.h>
#include "utils.h"

#define TAG "GtChat"
#include "gnome-twitch/gt-log.h"

//TODO: Replace copied text pixbufs with emoticon text

#define CHAT_DARK_THEME_CSS_CLASS "dark-theme"
#define CHAT_LIGHT_THEME_CSS_CLASS "light-theme"
#define CHAT_DARK_THEME_CSS ".gt-chat { background-color: rgba(25, 25, 31, %.2f); }"
#define CHAT_LIGHT_THEME_CSS ".gt-chat { background-color: rgba(242, 242, 242, %.2f); }"

#define MAX_SCROLLBACK 1000 //TODO: Make this a setting
#define SAVEUP_AMOUNT 100

const char* default_chat_colours[] =
{
    "#FF0000", "#0000FF", "#00FF00", "#B22222",
    "#FF7F50", "#9ACD32", "#FF4500", "#2E8B57",
    "#DAA520", "#D2691E", "#5F9EA0", "#1E90FF",
    "#FF69B4", "#8A2BE2", "#00FF7F"
};

static gchar* emote_replacement_codes[] =
{
    NULL, ":-)", ":-(", ":-D", ">(",
    ":-|", "o_O", "B-)", ":-O", "<3",
    ":-/", ";-)", ":-P", ";-P", "R-)",
};

typedef struct
{
    gboolean dark_theme;
    gdouble opacity;
    gchar* cur_theme;

    GtkWidget* emote_popover;
    GtkWidget* emote_flow;

    GtkWidget* error_label;
    GtkWidget* chat_view;
    GtkWidget* chat_scroll;
    GtkWidget* chat_scroll_vbar;
    GtkWidget* chat_entry;
    GtkTextBuffer* chat_buffer;
    GtkAdjustment* chat_adjustment;
    GtkTextTagTable* tag_table;
    GtkWidget* main_stack;
    GtkWidget* connecting_revealer;

    GtkTextMark* bottom_mark;
    GtkTextIter bottom_iter;

    GtkCssProvider* chat_css_provider;

    GtIrc* irc;
    GCancellable* irc_cancel;
    guint irc_disconnected_source;
    GtChannel* chan;

    gboolean chat_sticky;

    GRegex* url_regex;

    GMutex mutex;

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

    gt_irc_privmsg(priv->irc, msg);

    gtk_entry_set_text(GTK_ENTRY(priv->chat_entry), "");
}

static void
emote_activated_cb(GtkFlowBox* box,
                   GtkFlowBoxChild* child,
                   gpointer udata)
{
    GtChat* self = GT_CHAT(udata);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);
    GtkWidget* image = gtk_bin_get_child(GTK_BIN(child));
    const gchar* code = g_object_get_data(G_OBJECT(image), "code");
    const gchar* text = gtk_entry_get_text(GTK_ENTRY(priv->chat_entry));
    gchar* new_text = NULL;

    if (strlen(text) == 0 || text[strlen(text) - 1] == ' ')
        new_text = g_strdup_printf("%s%s ", text, code);
    else
        new_text = g_strdup_printf("%s %s ", text, code);

    gtk_entry_set_text(GTK_ENTRY(priv->chat_entry), new_text);

    g_free(new_text);
}

static void
emote_popup_closed_cb(GtkPopover* popover,
                      gpointer udata)
{
    REMOVE_STYLE_CLASS(udata, "popup-open");
}

static void
emote_icon_press_cb(GtkEntry* entry,
                    GtkEntryIconPosition* pos,
                    GdkEvent* evt,
                    gpointer udata)
{
    GtChat* self = GT_CHAT(udata);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);
    GdkRectangle rec;

    gtk_entry_get_icon_area(entry, GTK_ENTRY_ICON_SECONDARY, &rec);
    gtk_popover_set_pointing_to(GTK_POPOVER(priv->emote_popover), &rec);
    gtk_widget_show(priv->emote_popover);
    g_signal_connect(priv->emote_popover, "closed", G_CALLBACK(emote_popup_closed_cb), entry);

    ADD_STYLE_CLASS(entry, "popup-open");
}

static gint
int_compare(const GtChatEmote* a, const GtChatEmote* b)
{
    return a->id - b->id;
}

static void
emoticons_cb(GObject* source,
             GAsyncResult* res,
             gpointer udata)
{
    GtChat* self = GT_CHAT(udata);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);
    GError* error = NULL;
    GList* emoticons = NULL;

    emoticons = g_task_propagate_pointer(G_TASK(res), &error);

    if (error)
    {
        //TODO: Show this error to user
        WARNING("Couldn't get emoticons list");
        return;
    }

    utils_container_clear(GTK_CONTAINER(priv->emote_flow));

    emoticons = g_list_sort(emoticons, (GCompareFunc) int_compare);

    for (GList* l = emoticons; l != NULL; l = l->next)
    {
        GtChatEmote* emote = l->data;

        g_assert_nonnull(emote);
        g_assert(GDK_IS_PIXBUF(emote->pixbuf));

        GtkWidget* image = gtk_image_new_from_pixbuf(emote->pixbuf);
        gchar* code = NULL;;

        gtk_widget_set_visible(image, TRUE);

        if (emote->id < 15)
            code = emote_replacement_codes[emote->id];
        else
            code = emote->code;

        gtk_widget_set_tooltip_text(image, code);

        g_object_set_data_full(G_OBJECT(image), "code",
            g_strdup(code), g_free);

        gtk_flow_box_insert(GTK_FLOW_BOX(priv->emote_flow), image, -1);
    }

    gt_chat_emote_list_free(emoticons);
}

static gboolean
irc_source_cb(GtIrcMessage* msg,
              gpointer udata)
{
    GtChat* self = GT_CHAT(udata);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);
    gboolean ret = G_SOURCE_REMOVE;

    g_mutex_lock(&priv->mutex);

    if (msg->cmd_type == GT_IRC_COMMAND_PRIVMSG)
    {
        GtkTextIter iter;
        GtIrcCommandPrivmsg* privmsg = msg->cmd.privmsg;
        GtkTextTag* colour_tag;
        const gchar* sender;

        gtk_text_buffer_get_end_iter(priv->chat_buffer, &iter);

        //FIXME: Ideally the display name should be bold and the nick name should be normal,
        //will do this later
        if (utils_str_empty(privmsg->display_name))
            sender = g_strdup(msg->nick);
        else
        {
            g_assert(g_utf8_validate(privmsg->display_name, -1, NULL));

            for (const gchar* next_unichar = privmsg->display_name; *next_unichar; next_unichar = g_utf8_next_char(next_unichar))
            {
                GUnicodeScript script = g_unichar_get_script(g_utf8_get_char(next_unichar));

                switch (script)
                {
                    case G_UNICODE_SCRIPT_HIRAGANA:
                    case G_UNICODE_SCRIPT_KATAKANA:
                    case G_UNICODE_SCRIPT_HANGUL:
                    case G_UNICODE_SCRIPT_HAN:
                    {
                        sender = g_strdup_printf("%s (%s)", privmsg->display_name, msg->nick);
                            goto done;
                    }
                    default:
                        break;
                }
            }

            sender = g_strdup(privmsg->display_name);
        }

    done:

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

        for (GList* l = privmsg->badges; l != NULL; l = l->next)
        {
            g_assert_nonnull(l->data);

            GtChatBadge* badge = l->data;

            g_assert(GDK_IS_PIXBUF(badge->pixbuf));

            gtk_text_buffer_insert_pixbuf(priv->chat_buffer, &iter, GDK_PIXBUF(badge->pixbuf));
            gtk_text_buffer_insert(priv->chat_buffer, &iter, " ", -1);
        }

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
        for (gint i = 0; i < g_utf8_strlen(privmsg->msg, -1);
             ++i, c = g_utf8_offset_to_pointer(privmsg->msg, i),
                 d = g_utf8_offset_to_pointer(privmsg->msg, i + 1))
        {
            GtChatEmote* emote = NULL;

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

        gtk_text_buffer_get_end_iter(priv->chat_buffer, &iter);

        gtk_text_buffer_move_mark(priv->chat_buffer, priv->bottom_mark, &iter);

        if (priv->chat_sticky)
            gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(priv->chat_view), priv->bottom_mark);
    }
    else if (msg->cmd_type == GT_IRC_COMMAND_USERSTATE)
    {
        const gchar* emote_sets = utils_search_key_value_strv(msg->tags, "emote-sets");

        gt_twitch_emoticons_async(main_app->twitch, emote_sets,
            (GAsyncReadyCallback) emoticons_cb, NULL, self);
    }

    gt_irc_message_free(msg);

    g_mutex_unlock(&priv->mutex);

    return G_SOURCE_CONTINUE;
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

    GtChannel* open_chan = gt_player_get_channel(GT_WIN_ACTIVE->player);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->main_stack), "chatview");

    if (open_chan)
        gt_chat_connect(self, open_chan);
}

static void
reconnect_cb(GtkButton* button,
             gpointer udata)
{
    g_assert(GT_IS_CHAT(udata));

    GtChat* self = GT_CHAT(udata);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->main_stack), "chatview");

    gt_irc_connect_and_join_channel_async(priv->irc, priv->chan,
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

    GtIrcState state = gt_irc_get_state(priv->irc);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->connecting_revealer),
        state == GT_IRC_STATE_JOINED);
}

static void
after_connected_cb(GObject* source,
                   GParamSpec* pspec,
                   gpointer udata)
{
    GtChat* self = GT_CHAT(udata);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);

    GtIrcState state = gt_irc_get_state(priv->irc);

    if (state == GT_IRC_STATE_CONNECTED)
    {
        gtk_widget_set_sensitive(priv->chat_entry,
            gt_app_credentials_valid(main_app));
        gtk_entry_set_placeholder_text(GTK_ENTRY(priv->chat_entry),
            gt_app_credentials_valid(main_app)
            ? _("Send a message") : _("Please login to chat"));
    }
}

static void
irc_state_changed_cb(GObject* source,
    GParamSpec* pspec, gpointer udata)
{
    g_assert(GT_IS_CHAT(udata));
    g_assert(GT_IS_IRC(source));

    GtChat* self = GT_CHAT(udata);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);

    GtIrcState state = gt_irc_get_state(priv->irc);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->connecting_revealer),
        state != GT_IRC_STATE_JOINED);
    gtk_widget_set_sensitive(priv->chat_entry,
        state == GT_IRC_STATE_JOINED && gt_app_credentials_valid(main_app));
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

    g_object_unref(priv->irc);
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
                                                "/com/vinszent/GnomeTwitch/ui/gt-chat.ui");

    gtk_widget_class_bind_template_child_private(widget_class, GtChat, chat_view);
    gtk_widget_class_bind_template_child_private(widget_class, GtChat, chat_scroll);
    gtk_widget_class_bind_template_child_private(widget_class, GtChat, chat_entry);
    gtk_widget_class_bind_template_child_private(widget_class, GtChat, main_stack);
    gtk_widget_class_bind_template_child_private(widget_class, GtChat, error_label);
    gtk_widget_class_bind_template_child_private(widget_class, GtChat, connecting_revealer);
    gtk_widget_class_bind_template_child_private(widget_class, GtChat, emote_popover);
    gtk_widget_class_bind_template_child_private(widget_class, GtChat, emote_flow);
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
value_changed(GtkAdjustment* adjustment,
              gpointer udata)
{
    GtChat* self = GT_CHAT(udata);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);

    gdouble val = gtk_adjustment_get_value(adjustment);
    gdouble upper = gtk_adjustment_get_upper(adjustment);
    gdouble page_size = gtk_adjustment_get_page_size(adjustment);

    if (val >= upper - page_size - 5 || val < 1)
    {
        gint lc = gtk_text_buffer_get_line_count(priv->chat_buffer);

        if (lc - MAX_SCROLLBACK >= SAVEUP_AMOUNT)
        {
            g_mutex_lock(&priv->mutex);

            GtkTextIter start, end;

            gtk_text_buffer_get_iter_at_line(priv->chat_buffer, &start, 0);
            gtk_text_buffer_get_iter_at_line(priv->chat_buffer, &end, lc - MAX_SCROLLBACK);
            gtk_text_buffer_delete(priv->chat_buffer, &start, &end);

            g_mutex_unlock(&priv->mutex);
        }
    }
}

static void
gt_chat_init(GtChat* self)
{
    GtChatPrivate* priv = gt_chat_get_instance_private(self);

    gtk_widget_init_template(GTK_WIDGET(self));

    g_mutex_init(&priv->mutex);

    priv->chat_css_provider = gtk_css_provider_new();
    gtk_style_context_add_provider(gtk_widget_get_style_context(GTK_WIDGET(self)),
                                   GTK_STYLE_PROVIDER(priv->chat_css_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_USER);

    priv->chat_scroll_vbar = gtk_scrolled_window_get_vscrollbar(GTK_SCROLLED_WINDOW(priv->chat_scroll));
    priv->chat_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(priv->chat_view));
    priv->tag_table = gtk_text_buffer_get_tag_table(priv->chat_buffer);
    gtk_text_buffer_get_end_iter(priv->chat_buffer, &priv->bottom_iter);
    priv->bottom_mark = gtk_text_buffer_create_mark(priv->chat_buffer, "end", &priv->bottom_iter, TRUE);
    priv->chat_adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(priv->chat_scroll));

    priv->irc = gt_irc_new();
    priv->irc_cancel = g_cancellable_new();
    priv->irc_disconnected_source = 0;
    priv->chan = NULL;

    priv->chat_sticky = TRUE;

    priv->url_regex = g_regex_new("(https?://([-\\w\\.]+)+(:\\d+)?(/([\\w/_\\.]*(\\?\\S+)?)?)?)",
                                  G_REGEX_OPTIMIZE, 0, NULL);


    g_signal_connect(priv->chat_entry, "key-press-event", G_CALLBACK(key_press_cb), self);
    utils_signal_connect_oneshot(self, "hierarchy-changed", G_CALLBACK(anchored_cb), self);
    g_signal_connect(priv->irc, "error-encountered", G_CALLBACK(error_encountered_cb), self);
    g_signal_connect(priv->irc, "notify::state", G_CALLBACK(connected_cb), self);
    g_signal_connect_after(priv->irc, "notify::state", G_CALLBACK(after_connected_cb), self);
    g_signal_connect(priv->chat_scroll, "edge-reached", G_CALLBACK(edge_reached_cb), self);
    g_signal_connect(priv->chat_view, "button-press-event", G_CALLBACK(chat_view_button_press_cb), self);
    g_signal_connect(priv->chat_view, "motion-notify-event", G_CALLBACK(chat_view_motion_cb), self);
    g_signal_connect(priv->chat_scroll, "scroll-event", G_CALLBACK(chat_scrolled_cb), self);
    g_signal_connect(priv->chat_scroll_vbar, "button-press-event", G_CALLBACK(chat_scrolled_cb), self);
    g_signal_connect(priv->chat_adjustment, "value-changed", G_CALLBACK(value_changed), self);
    g_signal_connect(priv->chat_entry, "icon-press", G_CALLBACK(emote_icon_press_cb), self);
    g_signal_connect(priv->emote_flow, "child-activated", G_CALLBACK(emote_activated_cb), self);
    g_signal_connect(priv->irc, "notify::state", G_CALLBACK(irc_state_changed_cb), self);

    /* g_object_bind_property(priv->irc, "logged-in", */
    /*                        priv->connecting_revealer, "reveal-child", */
    /*                        G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN); */
    /* g_object_bind_property(priv->irc, "logged-in", */
    /*                        priv->chat_entry, "sensitive", */
    /*                        G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE); */

    g_source_set_callback((GSource*) priv->irc->source, (GSourceFunc) irc_source_cb, self, NULL);

    ADD_STYLE_CLASS(self, "gt-chat");
}

static void
disconnected_cb(GObject* source,
    GParamSpec* pspec, gpointer udata)
{
    g_assert(GT_IS_CHAT(udata));
    g_assert(GT_IS_IRC(source));

    GtChat* self = GT_CHAT(udata);
    GtChatPrivate* priv = gt_chat_get_instance_private(self);

    if (gt_irc_get_state(priv->irc) == GT_IRC_STATE_DISCONNECTED)
    {
        gt_irc_connect_and_join_channel_async(priv->irc, priv->chan,
            priv->irc_cancel, NULL, NULL);

        g_signal_handler_disconnect(priv->irc, priv->irc_disconnected_source);
        priv->irc_disconnected_source = 0;
    }
}

void
gt_chat_connect(GtChat* self, GtChannel* chan)
{
    g_assert(GT_IS_CHAT(self));
    g_assert(GT_IS_CHANNEL(chan));

    INFO("Connecting to channel %s", gt_channel_get_name(chan));

    GtChatPrivate* priv = gt_chat_get_instance_private(self);

    priv->chat_sticky = TRUE;

    priv->chan = g_object_ref(chan);

    GtIrcState state = gt_irc_get_state(priv->irc);

    utils_refresh_cancellable(&priv->irc_cancel);

    if (state >= GT_IRC_STATE_CONNECTING)
    {
        if (priv->irc_disconnected_source > 0)
            g_signal_handler_disconnect(priv->irc, priv->irc_disconnected_source);

        priv->irc_disconnected_source = g_signal_connect(priv->irc, "notify::state",
            G_CALLBACK(disconnected_cb), self);
    }
    else
    {
        gt_irc_connect_and_join_channel_async(priv->irc, priv->chan,
            priv->irc_cancel, NULL, NULL);
    }
}

void
gt_chat_disconnect(GtChat* self)
{
    INFO("Disconnecting");

    GtChatPrivate* priv = gt_chat_get_instance_private(self);

    GtIrcState state = gt_irc_get_state(priv->irc);

    if (priv->irc_disconnected_source > 0)
    {
        g_signal_handler_disconnect(priv->irc, priv->irc_disconnected_source);
        priv->irc_disconnected_source = 0;
    }

    if (state < GT_IRC_STATE_CONNECTING)
    {
        g_assert_null(priv->chan);

        return;
    }
    else if (state == GT_IRC_STATE_CONNECTING)
        g_cancellable_cancel(priv->irc_cancel);
    else if (state > GT_IRC_STATE_CONNECTING)
        gt_irc_disconnect(priv->irc);

    g_clear_object(&priv->chan);

    gtk_text_buffer_set_text(priv->chat_buffer, "", -1);
}
