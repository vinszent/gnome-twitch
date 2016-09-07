#include "gt-win.h"
#include <gdk/gdkx.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include "gt-twitch.h"
#include "gt-player.h"
#include "gt-player-header-bar.h"
#include "gt-browse-header-bar.h"
#include "gt-channels-view.h"
#include "gt-games-view.h"
#include "gt-follows-view.h"
#include "gt-settings-dlg.h"
#include "gt-twitch-login-dlg.h"
#include "gt-twitch-channel-info-dlg.h"
#include "gt-chat.h"
#include "gt-enums.h"
#include "config.h"
#include "version.h"
#include "utils.h"

#define TAG "GtWin"
#include "gnome-twitch/gt-log.h"

#define MAIN_VISIBLE_CHILD gtk_stack_get_visible_child(GTK_STACK(priv->main_stack))

typedef struct
{
    gchar* msg;
    GCallback cb;
    gpointer udata;
} QueuedInfoData;

typedef struct
{
    GtkWidget* main_stack;
    GtkWidget* channels_view;
    GtkWidget* games_view;
    GtkWidget* follows_view;
    GtkWidget* header_stack;
    GtkWidget* browse_stack;
    GtkWidget* player_header_bar;
    GtkWidget* browse_header_bar;
    GtkWidget* browse_stack_switcher;
    GtkWidget* chat_view;
    GtkWidget* player;

    GtkWidget* info_revealer;
    GtkWidget* info_label;
    GtkWidget* info_bar;
    GtkWidget* info_bar_yes_button;
    GtkWidget* info_bar_no_button;
    GtkWidget* info_bar_ok_button;
    GtkWidget* info_bar_details_button;
    GtkWidget* info_bar_close_button;

    QueuedInfoData* cur_info_data;
    GQueue* info_queue;

    GtSettingsDlg* settings_dlg;

    gboolean fullscreen;
} GtWinPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtWin, gt_win, GTK_TYPE_APPLICATION_WINDOW)

enum
{
    PROP_0,
    PROP_CHANNELS_VIEW,
    PROP_GAMES_VIEW,
    PROP_VISIBLE_VIEW,
    PROP_FULLSCREEN,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtWin*
gt_win_new(GtApp* app)
{
    return g_object_new(GT_TYPE_WIN,
                        "application", app,
                        "show-menubar", FALSE,
                        NULL);
}

static void
show_about_cb(GSimpleAction* action,
              GVariant* par,
              gpointer udata)
{
    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);
    GtkWidget* about_dlg = NULL;

    const char* authors[] = {"Vincent Szolnoky", NULL};
    const char* contributors[] = {"Dimitrios Christidis", NULL};

    about_dlg = gtk_about_dialog_new();

    g_object_set(about_dlg,
                 "version", GT_VERSION,
                 "program-name", "GNOME Twitch",
                 "authors", &authors,
                 "license-type", GTK_LICENSE_GPL_3_0,
                 "copyright", "Copyright © 2015 Vincent Szolnoky",
                 "comments", _("Enjoy Twitch on your GNU/Linux desktop"),
                 "logo-icon-name", "com.vinszent.GnomeTwitch",
                 "website", "https://github.com/vinszent/gnome-twitch",
                 "website-label", "GitHub",
                 // Translators: Put your details here :)
                 "translator-credits", _("translator-credits"),
                 NULL);

    gtk_about_dialog_add_credit_section(GTK_ABOUT_DIALOG(about_dlg), _("Contributors"), contributors);
    gtk_window_set_transient_for(GTK_WINDOW(about_dlg), GTK_WINDOW(self));

    gtk_dialog_run(GTK_DIALOG(about_dlg));
    gtk_widget_destroy(about_dlg);
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

    if (par)
    {
        GEnumClass* eclass = g_type_class_ref(GT_TYPE_SETTINGS_DLG_VIEW);
        GEnumValue* eval = g_enum_get_value_by_nick(eclass, g_variant_get_string(par, NULL));

        gt_settings_dlg_set_view(priv->settings_dlg, eval->value);

        g_simple_action_set_state(action, par);

        g_type_class_unref(eclass);
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
}

static void
show_error_dialogue_cb(GtkInfoBar* info_bar,
                       gint res,
                       gchar** udata)
{
    GtkBuilder* builder = gtk_builder_new_from_resource("/com/vinszent/GnomeTwitch/ui/gt-error-dlg.ui");
    GtkWidget* dlg = GTK_WIDGET(gtk_builder_get_object(builder, "dlg"));
    GtkTextView* details_text_view = GTK_TEXT_VIEW(gtk_builder_get_object(builder, "details_text_view"));

    if (res != GTK_RESPONSE_OK) goto cleanup;

    gtk_text_buffer_set_text(gtk_text_view_get_buffer(details_text_view),
                             *(udata+1), -1);

    g_object_set(dlg,
                 "text", _("Something went wrong"),
                 "secondary-text", *udata,
                 NULL);
    g_signal_connect(dlg, "response", G_CALLBACK(gtk_widget_destroy), NULL);

    gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(GT_WIN_ACTIVE));
    gtk_widget_show(dlg);

cleanup:
    g_strfreev(udata);
}


static void
show_info_bar(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);
    QueuedInfoData* data = g_queue_pop_head(priv->info_queue);

    if (data)
    {
        if (data->cb)
        {
            if (data->cb == G_CALLBACK(show_error_dialogue_cb))
            {
                gtk_widget_set_visible(priv->info_bar_ok_button, FALSE);
                gtk_widget_set_visible(priv->info_bar_yes_button, FALSE);
                gtk_widget_set_visible(priv->info_bar_no_button, FALSE);
                gtk_widget_set_visible(priv->info_bar_details_button, TRUE);
                gtk_widget_set_visible(priv->info_bar_close_button, TRUE);
                gtk_label_set_markup(GTK_LABEL(priv->info_label), data->msg);
                gtk_info_bar_set_message_type(GTK_INFO_BAR(priv->info_bar), GTK_MESSAGE_ERROR);

                g_signal_connect(priv->info_bar, "response", G_CALLBACK(data->cb), data->udata);
            }
            else
            {

                g_signal_connect(priv->info_bar, "response", G_CALLBACK(data->cb), data->udata);

                gtk_widget_set_visible(priv->info_bar_ok_button, FALSE);
                gtk_widget_set_visible(priv->info_bar_yes_button, TRUE);
                gtk_widget_set_visible(priv->info_bar_no_button, TRUE);
                gtk_widget_set_visible(priv->info_bar_details_button, FALSE);
                gtk_widget_set_visible(priv->info_bar_close_button, FALSE);
                gtk_label_set_markup(GTK_LABEL(priv->info_label), data->msg);
                gtk_info_bar_set_message_type(GTK_INFO_BAR(priv->info_bar), GTK_MESSAGE_QUESTION);
            }
        }
        else
        {
            gtk_widget_set_visible(priv->info_bar_yes_button, FALSE);
            gtk_widget_set_visible(priv->info_bar_no_button, FALSE);
            gtk_widget_set_visible(priv->info_bar_ok_button, TRUE);
            gtk_widget_set_visible(priv->info_bar_details_button, FALSE);
            gtk_widget_set_visible(priv->info_bar_close_button, FALSE);
            gtk_label_set_markup(GTK_LABEL(priv->info_label), data->msg);
            gtk_info_bar_set_message_type(GTK_INFO_BAR(priv->info_bar), GTK_MESSAGE_INFO);
        }

        priv->cur_info_data = data;

        gtk_revealer_set_reveal_child(GTK_REVEALER(priv->info_revealer), TRUE);
    }
    else
    {
        priv->cur_info_data = NULL;

        gtk_revealer_set_reveal_child(GTK_REVEALER(priv->info_revealer), FALSE);
    }
}

static void
close_info_bar_cb(GtkInfoBar* bar,
                  gint res,
                  gpointer udata)
{
    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    if (priv->cur_info_data)
    {
        if (priv->cur_info_data->cb)
        {
            g_signal_handlers_disconnect_by_func(priv->info_bar,
                                                 priv->cur_info_data->cb,
                                                 priv->cur_info_data->udata);
        }

        g_free(priv->cur_info_data->msg);
        g_free(priv->cur_info_data);
    }

    show_info_bar(self);
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
        gt_win_ask_question(self, _("Already logged into Twitch, refresh login?"),
                            G_CALLBACK(refresh_login_cb), self);
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
    GtChannel* channel;
    const gchar* name;

    g_object_get(self->player, "channel", &channel, NULL);

    name = gt_channel_get_name(channel);

    g_message("{GtWin} Showing channel info for '%s'", name);

    gtk_window_present(GTK_WINDOW(dlg));

    gt_twitch_channel_info_dlg_load_channel(dlg, name);

    g_object_unref(channel);
}


static void
refresh_view_cb(GSimpleAction* action,
                GVariant* arg,
                gpointer udata)
{
    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);
    GtkWidget* visible_child = gtk_stack_get_visible_child(GTK_STACK(priv->browse_stack));

    if (visible_child == priv->channels_view)
        gt_channels_view_refresh(GT_CHANNELS_VIEW(priv->channels_view));
    else if (visible_child == priv->games_view)
        gt_games_view_refresh(GT_GAMES_VIEW(priv->games_view));
    else if (visible_child == priv->follows_view)
    {
        //TODO: Quick hack, turn this into a proper refresh function
        if (gt_app_credentials_valid(main_app))
            gt_follows_manager_load_from_twitch(main_app->fav_mgr);
        else
            gt_follows_manager_load_from_file(main_app->fav_mgr);
    }
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
    GdkModifierType modifiers = gtk_accelerator_get_default_mod_mask();
    gboolean playing;
    GAction *action;

    g_object_get(self->player, "playing", &playing, NULL);

    if (MAIN_VISIBLE_CHILD == GTK_WIDGET(self->player))
    {
        if (evt->keyval == GDK_KEY_Escape)
        {
            if (priv->fullscreen)
                g_object_set(self, "fullscreen", FALSE, NULL);
            else
            {
                action = g_action_map_lookup_action(G_ACTION_MAP(self), "close_player");
                g_action_activate(action, NULL);
            }
        }
        else if (evt->keyval == GDK_KEY_f)
        {
            g_object_set(self, "fullscreen", !priv->fullscreen, NULL);
        }
    }
    else
    {
        if (evt->keyval == GDK_KEY_Escape)
            gt_browse_header_bar_stop_search(GT_BROWSE_HEADER_BAR(priv->browse_header_bar));
        else if (evt->keyval == GDK_KEY_f && (evt->state & modifiers) == GDK_CONTROL_MASK)
            gt_browse_header_bar_toggle_search(GT_BROWSE_HEADER_BAR(priv->browse_header_bar));
        else
        {
            GtkWidget* view = gtk_stack_get_visible_child(GTK_STACK(priv->browse_stack));

            if (view == priv->channels_view)
                gt_channels_view_handle_event(GT_CHANNELS_VIEW(priv->channels_view), (GdkEvent*) evt);
            else if (view == priv->games_view)
                gt_games_view_handle_event(GT_GAMES_VIEW(priv->games_view), (GdkEvent*) evt);
            else if (view == priv->follows_view)
                gt_follows_view_handle_event(GT_FOLLOWS_VIEW(priv->follows_view), (GdkEvent* )evt);
        }
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

static void
update_fullscreen(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    if (priv->fullscreen)
        gtk_window_fullscreen(GTK_WINDOW(self));
    else
        gtk_window_unfullscreen(GTK_WINDOW(self));
}

static GActionEntry win_actions[] =
{
    {"refresh_view", refresh_view_cb, NULL, NULL, NULL},
    {"show_view_default", show_view_default_cb, NULL, NULL, NULL},
    {"show_about", show_about_cb, NULL, NULL, NULL},
    {"show_settings", show_settings_cb, NULL, NULL},
    {"show_settings_with_view", NULL, "s", "'general'", show_settings_cb},
    {"show_twitch_login", show_twitch_login_cb, NULL, NULL, NULL},
    {"show_channel_info", show_channel_info_cb, NULL, NULL, NULL},
    {"close_player", close_player_cb, NULL, NULL, NULL},
};

static void
finalize(GObject* object)
{
    GtWin* self = (GtWin*) object;
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    G_OBJECT_CLASS(gt_win_parent_class)->finalize(object);

    g_message("{GtWin} Finalise");
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
        case PROP_FULLSCREEN:
            priv->fullscreen = g_value_get_boolean(val);
            update_fullscreen(self);
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
    props[PROP_VISIBLE_VIEW] = g_param_spec_object("visible-view",
                                                   "Visible View",
                                                   "Visible View",
                                                   GTK_TYPE_WIDGET,
                                                   G_PARAM_READWRITE);
    props[PROP_FULLSCREEN] = g_param_spec_boolean("fullscreen",
                                                  "Fullscreen",
                                                  "Whether window is fullscreen",
                                                  FALSE,
                                                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    g_object_class_install_properties(object_class,
                                      NUM_PROPS,
                                      props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
                                                "/com/vinszent/GnomeTwitch/ui/gt-win.ui");
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, main_stack);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, channels_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, games_view);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), GtWin, player);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, header_stack);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, player_header_bar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, browse_header_bar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, browse_stack);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, browse_stack_switcher);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, follows_view);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, info_revealer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, info_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, info_bar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, info_bar_yes_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, info_bar_no_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, info_bar_ok_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, info_bar_details_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, info_bar_close_button);
}

static void
gt_win_init(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);
    GPropertyAction* action;

    GT_TYPE_PLAYER; // Hack to load GtPlayer into the symbols table
    GT_TYPE_PLAYER_HEADER_BAR;
    GT_TYPE_BROWSE_HEADER_BAR;
    GT_TYPE_CHANNELS_VIEW;
    GT_TYPE_GAMES_VIEW;
    GT_TYPE_FOLLOWS_VIEW;
    GT_TYPE_CHAT;

    gtk_window_set_application(GTK_WINDOW(self), GTK_APPLICATION(main_app));

    gtk_widget_init_template(GTK_WIDGET(self));
    gtk_widget_realize(priv->player_header_bar);

    priv->cur_info_data = NULL;
    priv->info_queue = g_queue_new();

    gtk_window_set_default_size(GTK_WINDOW(self),
                                g_settings_get_int(main_app->settings, "window-width"),
                                g_settings_get_int(main_app->settings, "window-height"));

    gtk_window_set_default_icon_name("com.vinszent.GnomeTwitch");

    g_object_bind_property(priv->browse_stack, "visible-child",
                           self, "visible-view",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

    GdkScreen* screen = gdk_screen_get_default();
    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(css, "/com/vinszent/GnomeTwitch/com.vinszent.GnomeTwitch.style.css");
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(css),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_signal_connect_after(self, "key-press-event", G_CALLBACK(key_press_cb), self);
    g_signal_connect(self, "delete-event", G_CALLBACK(delete_cb), self);
    g_signal_connect_after(priv->info_bar, "response", G_CALLBACK(close_info_bar_cb), self);

    g_action_map_add_action_entries(G_ACTION_MAP(self),
                                    win_actions,
                                    G_N_ELEMENTS(win_actions),
                                    self);

    action = g_property_action_new("toggle_fullscreen", self, "fullscreen");
    g_action_map_add_action(G_ACTION_MAP(self),
                            G_ACTION(action));
    g_object_unref(action);

    GtkWindowGroup* window_group = gtk_window_group_new();
    gtk_window_group_add_window(window_group, GTK_WINDOW(self));
    g_object_unref(window_group);
}

//TODO: Make this action
void
gt_win_open_channel(GtWin* self, GtChannel* chan)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

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
gt_win_is_fullscreen(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    return priv->fullscreen;
}

void
gt_win_toggle_fullscreen(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    g_object_set(self, "fullscreen", !priv->fullscreen, NULL);
}

void
gt_win_show_info_message(GtWin* self, const gchar* msg)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);
    QueuedInfoData* data = g_new0(QueuedInfoData, 1);

    data->msg = g_strdup(msg);

    g_queue_push_tail(priv->info_queue, data);

    if (!gtk_revealer_get_reveal_child(GTK_REVEALER(priv->info_revealer)))
        show_info_bar(self);
}

void
gt_win_show_error_message(GtWin* self, const gchar* secondary, const gchar* details)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);
    QueuedInfoData* data = g_new(QueuedInfoData, 1);
    gchar** udata = g_malloc(sizeof(gchar*)*3);
    // Translators: Please keep the markup tags
    gchar* msg = g_strdup_printf(_("<b>Something went wrong:</b> %s."), secondary);

    *udata = g_strdup(secondary);
    *(udata+1) = g_strdup(details);
    *(udata+2) = NULL;

    gt_win_ask_question(self, msg, G_CALLBACK(show_error_dialogue_cb), udata);
}

void
gt_win_ask_question(GtWin* self, const gchar* msg, GCallback cb, gpointer udata)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);
    QueuedInfoData* data = g_new(QueuedInfoData, 1);

    data->msg = g_strdup(msg);
    data->cb = cb;
    data->udata = udata;

    g_queue_push_tail(priv->info_queue, data);

    if (!gtk_revealer_get_reveal_child(GTK_REVEALER(priv->info_revealer)))
        show_info_bar(self);
}
