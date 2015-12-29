#include "gt-win.h"
#include <gdk/gdkx.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include "gt-twitch.h"
#include "gt-player.h"
#include "gt-player-clutter.h"
//#include "gt-player-mpv.h"
#include "gt-player-header-bar.h"
#include "gt-browse-header-bar.h"
#include "gt-channels-view.h"
#include "gt-games-view.h"
#include "gt-favourites-view.h"
#include "gt-settings-dlg.h"
#include "gt-twitch-login-dlg.h"
#include "gt-twitch-channel-info-dlg.h"
#include "gt-twitch-chat-view.h"
#include "gt-enums.h"
#include "utils.h"
#include "config.h"
#include "version.h"

#define MAIN_VISIBLE_CHILD gtk_stack_get_visible_child(GTK_STACK(priv->main_stack))

typedef struct
{
    GtkWidget* main_stack;
    GtkWidget* channels_view;
    GtkWidget* games_view;
    GtkWidget* favourites_view;
    GtkWidget* header_stack;
    GtkWidget* browse_stack;
    GtkWidget* player_header_bar;
    GtkWidget* browse_header_bar;
    GtkWidget* browse_stack_switcher;
    GtkWidget* chat_view;

    GtkWidget* info_revealer;
    GtkWidget* info_label;
    GtkWidget* info_bar;
    GtkWidget* info_bar_yes_button;

    GtSettingsDlg* settings_dlg;

    gboolean fullscreen;

    GtChannel* open_channel;
} GtWinPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtWin, gt_win, GTK_TYPE_APPLICATION_WINDOW)

enum
{
    PROP_0,
    PROP_CHANNELS_VIEW,
    PROP_GAMES_VIEW,
    PROP_FULLSCREEN,
    PROP_VISIBLE_VIEW,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtWin*
gt_win_new(GtApp* app)
{
    return g_object_new(GT_TYPE_WIN,
                        "application", app,
                        NULL);
}

static void
show_about_cb(GSimpleAction* action,
              GVariant* par,
              gpointer udata)
{
    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    char* authors[] = {"Vincent Szolnoky", NULL};

    gtk_show_about_dialog(GTK_WINDOW(self),
                          "version", GT_VERSION,
                          "program-name", "GNOME Twitch",
                          "authors", &authors,
                          "license-type", GTK_LICENSE_GPL_3_0,
                          "copyright", "Copyright © 2015 Vincent Szolnoky",
                          "comments", _("Enjoy Twitch on your GNU/Linux desktop"),
                          "logo-icon-name", "gnome-twitch",
                          "website", "https://github.com/Ippytraxx/gnome-twitch",
                          "website-label", "GitHub",
                          "translator-credits", _("translator-credits"),
                          NULL);
}

static void
show_settings_cb(GSimpleAction* action,
                 GVariant* par,
                 gpointer udata)
{
    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    if (!priv->settings_dlg)
    {
        priv->settings_dlg = gt_settings_dlg_new(self);
        g_object_add_weak_pointer(G_OBJECT(priv->settings_dlg), (gpointer *) &priv->settings_dlg);
    }

    gtk_window_present(GTK_WINDOW(priv->settings_dlg));
}

static void
refresh_login_cb(GtkInfoBar* info_bar,
                 gint res,
                 gpointer udata)
{
    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    switch (res)
    {
        case GTK_RESPONSE_YES:
            gtk_window_present(GTK_WINDOW(gt_twitch_login_dlg_new(GTK_WINDOW(self))));
            break;
    }

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->info_revealer), FALSE);
    gtk_widget_set_visible(priv->info_bar_yes_button, FALSE);
    g_signal_handlers_disconnect_by_func(info_bar, refresh_login_cb, udata);
}

static void
close_info_bar_cb(GtkInfoBar* bar,
                  gint res,
                  gpointer udata)
{
    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    if (res == GTK_RESPONSE_CLOSE)
        gtk_revealer_set_reveal_child(GTK_REVEALER(priv->info_revealer), FALSE);
}


static void
show_twitch_login_cb(GSimpleAction* action,
                     GVariant* par,
                     gpointer udata)
{
    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);
    gchar* oauth_token;
    gchar* user_name;

    g_object_get(main_app,
                 "oauth-token", &oauth_token,
                 "user-name", &user_name,
                 NULL);

    if (oauth_token && user_name &&
        strlen(oauth_token) > 0 && strlen(user_name) > 0)
    {
        gtk_widget_set_visible(priv->info_bar_yes_button, TRUE);
        gtk_label_set_text(GTK_LABEL(priv->info_label), _("Already logged into Twitch, refresh login?"));
        gtk_info_bar_set_message_type(GTK_INFO_BAR(priv->info_bar), GTK_MESSAGE_QUESTION);
        gtk_revealer_set_reveal_child(GTK_REVEALER(priv->info_revealer), TRUE);

        g_signal_connect(priv->info_bar, "response", G_CALLBACK(refresh_login_cb), self);
    }
    else
    {
        GtTwitchLoginDlg* dlg = gt_twitch_login_dlg_new(GTK_WINDOW(self));

        gtk_window_present(GTK_WINDOW(dlg));
    }
}

static void
show_channel_info_cb(GSimpleAction* action,
                     GVariant* arg,
                     gpointer udata)
{
    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);
    GtTwitchChannelInfoDlg* dlg = gt_twitch_channel_info_dlg_new(GTK_WINDOW(self));
    const gchar* chan;

    chan = gt_channel_get_name(priv->open_channel);

    g_message("{GtWin} Showing channel info for '%s'", chan);

    gtk_window_present(GTK_WINDOW(dlg));

    gt_twitch_channel_info_dlg_load_channel(dlg, chan);
}


static void
refresh_view_cb(GSimpleAction* action,
                GVariant* arg,
                gpointer udata)
{
    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    if (gtk_stack_get_visible_child(GTK_STACK(priv->browse_stack)) == priv->channels_view)
        gt_channels_view_refresh(GT_CHANNELS_VIEW(priv->channels_view));
    else if (gtk_stack_get_visible_child(GTK_STACK(priv->browse_stack)) == priv->games_view)
        gt_games_view_refresh(GT_GAMES_VIEW(priv->games_view));
}

static void
show_view_default_cb(GSimpleAction* action,
                     GVariant* arg,
                     gpointer udata)
{
    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    if (gtk_stack_get_visible_child(GTK_STACK(priv->browse_stack)) == priv->channels_view)
        gt_channels_view_show_type(GT_CHANNELS_VIEW(priv->channels_view), GT_CHANNELS_CONTAINER_TYPE_TOP);
    else if (gtk_stack_get_visible_child(GTK_STACK(priv->browse_stack)) == priv->games_view)
        gt_games_view_show_type(GT_GAMES_VIEW(priv->games_view), GT_GAMES_CONTAINER_TYPE_TOP);
}

static gboolean
key_press_cb(GtkWidget* widget,
             GdkEventKey* evt,
             gpointer udata)
{
    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);
    gboolean playing;

    g_object_get(self->player, "playing", &playing, NULL);

    if (evt->keyval == GDK_KEY_space)
    {
        if (MAIN_VISIBLE_CHILD == self->player)
        {
            if (playing)
                gt_player_stop(GT_PLAYER(self->player));
            else
                gt_player_play(GT_PLAYER(self->player));
        }
    }
    else if (evt->keyval == GDK_KEY_Escape)
    {
        if (MAIN_VISIBLE_CHILD == self->player)
            if (priv->fullscreen)
                gtk_window_unfullscreen(GTK_WINDOW(self));
    }

    return FALSE;
}

static gboolean
delete_cb(GtkWidget* widget,
          GdkEvent* evt,
          gpointer udata)
{
    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);
    int width;
    int height;

    gtk_window_get_size(GTK_WINDOW(self), &width, &height);

    g_settings_set_int(main_app->settings, "window-width", width);
    g_settings_set_int(main_app->settings, "window-height", height);

    return FALSE;
}

static void
close_player_cb(GSimpleAction* action,
                GVariant* arg,
                gpointer udata)
{
    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    gt_player_close_channel(GT_PLAYER(self->player));

    gtk_stack_set_visible_child_name(GTK_STACK(priv->header_stack),
                                     "browse");
    gtk_stack_set_visible_child_name(GTK_STACK(priv->main_stack),
                                     "browse");
}

static GActionEntry win_actions[] =
{
    {"refresh_view", refresh_view_cb, NULL, NULL, NULL},
    {"show_view_default", show_view_default_cb, NULL, NULL, NULL},
    {"show_about", show_about_cb, NULL, NULL, NULL},
    {"show_settings", show_settings_cb, NULL, NULL, NULL},
    {"show_twitch_login", show_twitch_login_cb, NULL, NULL, NULL},
    {"show_channel_info", show_channel_info_cb, NULL, NULL, NULL},
    {"close_player", close_player_cb, NULL, NULL, NULL},
};

static gboolean
window_state_cb(GtkWidget* widget,
                GdkEventWindowState* evt,
                gpointer udata)
{
    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    if (evt->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
    {
        if (evt->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)
            priv->fullscreen = TRUE;
        else
            priv->fullscreen = FALSE;
        g_object_notify_by_pspec(G_OBJECT(self), props[PROP_FULLSCREEN]);
    }

    return FALSE;
}

static void
finalize(GObject* object)
{
    GtWin* self = (GtWin*) object;
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    G_OBJECT_CLASS(gt_win_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtWin* self = GT_WIN(obj);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    switch (prop)
    {
        case PROP_CHANNELS_VIEW:
            g_value_set_object(val, priv->channels_view);
            break;
        case PROP_GAMES_VIEW:
            g_value_set_object(val, priv->games_view);
            break;
        case PROP_FULLSCREEN:
            g_value_set_boolean(val, priv->fullscreen);
            break;
        case PROP_VISIBLE_VIEW:
            g_value_set_object(val, gtk_stack_get_visible_child(GTK_STACK(priv->browse_stack)));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
set_property(GObject*      obj,
             guint         prop,
             const GValue* val,
             GParamSpec*   pspec)
{
    GtWin* self = GT_WIN(obj);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    switch (prop)
    {
        case PROP_VISIBLE_VIEW:
            // Do nothing
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_win_class_init(GtWinClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_CHANNELS_VIEW] = g_param_spec_object("channels-view",
                                                    "Channels View",
                                                    "Channels View",
                                                    GT_TYPE_CHANNELS_VIEW,
                                                    G_PARAM_READABLE);
    props[PROP_GAMES_VIEW] = g_param_spec_object("games-view",
                                                 "Games View",
                                                 "Games View",
                                                 GT_TYPE_GAMES_VIEW,
                                                 G_PARAM_READABLE);
    props[PROP_FULLSCREEN] = g_param_spec_boolean("fullscreen",
                                                  "Fullscreen",
                                                  "Whether window is fullscreen",
                                                  FALSE,
                                                  G_PARAM_READABLE);
    props[PROP_VISIBLE_VIEW] = g_param_spec_object("visible-view",
                                                   "Visible View",
                                                   "Visible View",
                                                   GTK_TYPE_WIDGET,
                                                   G_PARAM_READWRITE);

    g_object_class_install_properties(object_class,
                                      NUM_PROPS,
                                      props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
                                                "/com/gnome-twitch/ui/gt-win.ui");
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, main_stack);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, channels_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, games_view);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), GtWin, player);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, header_stack);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, player_header_bar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, browse_header_bar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, browse_stack);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, browse_stack_switcher);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, favourites_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, info_revealer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, info_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, info_bar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, info_bar_yes_button);
}

static void
gt_win_init(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    GT_TYPE_PLAYER; // Hack to load GtPlayer into the symbols table
    GT_TYPE_PLAYER_CLUTTER;
//    GT_TYPE_PLAYER_MPV;
    GT_TYPE_PLAYER_HEADER_BAR;
    GT_TYPE_BROWSE_HEADER_BAR;
    GT_TYPE_CHANNELS_VIEW;
    GT_TYPE_GAMES_VIEW;
    GT_TYPE_FAVOURITES_VIEW;
    GT_TYPE_TWITCH_CHAT_VIEW;

//    self->open_channel = NULL;

    gtk_window_set_default_size(GTK_WINDOW(self),
                                g_settings_get_int(main_app->settings, "window-width"),
                                g_settings_get_int(main_app->settings, "window-height"));

    gtk_widget_init_template(GTK_WIDGET(self));

//    g_object_set(self, "application", main_app, NULL); // Another hack because GTK is bugged and resets the app menu when using custom widgets

    g_object_bind_property(priv->browse_stack, "visible-child",
                           self, "visible-view",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

    GdkScreen* screen = gdk_screen_get_default();
    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(css, "/com/gnome-twitch/gnome-twitch-style.css");
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_signal_connect(self, "window-state-event", G_CALLBACK(window_state_cb), self);
    g_signal_connect_after(self, "key-press-event", G_CALLBACK(key_press_cb), self);
    g_signal_connect(self, "delete-event", G_CALLBACK(delete_cb), self);
    g_signal_connect(priv->info_bar, "response", G_CALLBACK(close_info_bar_cb), self);

    g_action_map_add_action_entries(G_ACTION_MAP(self),
                                    win_actions,
                                    G_N_ELEMENTS(win_actions),
                                    self);

    GtkWindowGroup* window_group = gtk_window_group_new();
    gtk_window_group_add_window(window_group, GTK_WINDOW(self));
    g_object_unref(window_group);
}

//TODO: Make this action
void
gt_win_open_channel(GtWin* self, GtChannel* chan)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    g_clear_object(&priv->open_channel);
    g_object_ref(chan);
    priv->open_channel = chan;

    gt_player_open_channel(GT_PLAYER(self->player), chan);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->main_stack),
                                     "player");
    gtk_stack_set_visible_child_name(GTK_STACK(priv->header_stack),
                                     "player");
}

//TODO: Make these actions
void
gt_win_browse_view(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->header_stack),
                                     "browse");
    gtk_stack_set_visible_child_name(GTK_STACK(priv->main_stack),
                                     "browse");
}

void
gt_win_browse_channels_view(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    gt_win_browse_view(self);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->browse_stack),
                                     "channels");
}

void
gt_win_browse_games_view(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    gt_win_browse_view(self);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->browse_stack),
                                     "games");
}

GtChannelsView*
gt_win_get_channels_view(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    return GT_CHANNELS_VIEW(priv->channels_view);
}

GtGamesView*
gt_win_get_games_view(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    return GT_GAMES_VIEW(priv->games_view);
}

gboolean
gt_win_get_fullscreen(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    return priv->fullscreen;
}

void
gt_win_show_info_message(GtWin* self, const gchar* msg)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    gtk_label_set_text(GTK_LABEL(priv->info_label), msg);
    gtk_info_bar_set_message_type(GTK_INFO_BAR(priv->info_bar), GTK_MESSAGE_INFO);
    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->info_revealer), TRUE);
}
