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

#include "gt-win.h"
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include "gt-twitch.h"
#include "gt-player.h"
#include "gt-player-header-bar.h"
#include "gt-browse-header-bar.h"
#include "gt-channel-header-bar.h"
#include "gt-channel-container-view.h"
#include "gt-followed-container-view.h"
#include "gt-game-container-view.h"
#include "gt-channel-vod-container.h"
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

typedef struct
{
    gchar* msg;
    GCallback cb;
    gpointer udata;
} QueuedInfoData;

typedef struct
{
    GtkWidget* main_stack;
    GtkWidget* header_stack;
    GtkWidget* browse_stack;
    GtkWidget* player_header_bar;
    GtkWidget* browse_header_bar;
    GtkWidget* browse_stack_switcher;
    GtkWidget* chat_view;
    GtkWidget* player;
    GtkWidget* channel_stack;
    GtkWidget* channel_vod_container;
    GtkWidget* channel_header_bar;

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

    GBinding* search_binding;
    GBinding* back_binding;
} GtWinPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtWin, gt_win, GTK_TYPE_APPLICATION_WINDOW)

enum
{
    PROP_0,
    PROP_FULLSCREEN,
    PROP_OPEN_CHANNEL,
    NUM_PROPS,
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
container_view_changed_cb(GObject* source,
    GParamSpec* pspec, gpointer udata)
{
    g_assert(GT_IS_WIN(udata));

    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    GtkWidget* current_view = gtk_stack_get_visible_child(GTK_STACK(priv->browse_stack));

    g_assert(GT_IS_CONTAINER_VIEW(current_view));

    g_clear_object(&priv->back_binding);
    g_clear_object(&priv->search_binding);

    g_object_set(priv->browse_header_bar,
        "search-active",
        gt_container_view_get_search_active(GT_CONTAINER_VIEW(current_view)),
        "show-back-button",
        gt_container_view_get_show_back_button(GT_CONTAINER_VIEW(current_view)),
        NULL);

    priv->search_binding = g_object_bind_property(priv->browse_header_bar, "search-active",
        current_view, "search-active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

    priv->back_binding = g_object_bind_property(current_view, "show-back-button",
        priv->browse_header_bar, "show-back-button", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
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
                 "copyright", "Copyright © 2017 Vincent Szolnoky",
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
    gint res, gchar** udata)
{
    GtkBuilder* builder = gtk_builder_new_from_resource("/com/vinszent/GnomeTwitch/ui/gt-error-dlg.ui");
    GtkWidget* dlg = GTK_WIDGET(gtk_builder_get_object(builder, "dlg"));
    GtkTextView* details_text_view = GTK_TEXT_VIEW(gtk_builder_get_object(builder, "details_text_view"));
    GtkLabel* sub_label = GTK_LABEL(gtk_builder_get_object(builder, "error_label"));
    GtkWidget* close_button = GTK_WIDGET(gtk_builder_get_object(builder, "close_button"));
    GtkWidget* report_button = GTK_WIDGET(gtk_builder_get_object(builder, "report_button"));
    g_autofree gchar* info_text = NULL;

    if (res != GTK_RESPONSE_OK) goto cleanup;

    gtk_text_buffer_set_text(gtk_text_view_get_buffer(details_text_view),
                             *(udata+1), -1);

    info_text = g_strdup_printf(_("<b>Error message:</b> %s"), *udata);

    gtk_label_set_label(sub_label, info_text);

    g_signal_connect_swapped(close_button, "clicked", G_CALLBACK(gtk_widget_destroy), dlg);

    //TODO: Hook up report button

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

    if (gt_app_is_logged_in(main_app))
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
go_back_cb(GSimpleAction* action,
    GVariant* arg, gpointer udata)
{
    g_assert(GT_IS_WIN(udata));

    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    GtkWidget* current_view = gtk_stack_get_visible_child(GTK_STACK(priv->browse_stack));

    g_assert(GT_IS_CONTAINER_VIEW(current_view));

    gt_container_view_go_back(GT_CONTAINER_VIEW(current_view));
}

static void
refresh_cb(GSimpleAction* action,
    GVariant* arg, gpointer udata)
{
    g_assert(GT_IS_WIN(udata));

    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    GtkWidget* current_view = gtk_stack_get_visible_child(GTK_STACK(priv->browse_stack));

    g_assert(GT_IS_CONTAINER_VIEW(current_view));

    gt_container_view_refresh(GT_CONTAINER_VIEW(current_view));
}

static gboolean
key_press_cb(GtkWidget* widget,
             GdkEventKey* evt,
             gpointer udata)
{
    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);
    GdkModifierType modifiers = gtk_accelerator_get_default_mod_mask();
    GtkWidget* visible_child = gtk_stack_get_visible_child(GTK_STACK(priv->main_stack));
    gboolean playing;
    GAction *action = NULL;

    g_object_get(self->player, "playing", &playing, NULL);

    if (visible_child == GTK_WIDGET(self->player))
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
            /* GtkWidget* view = gtk_stack_get_visible_child(GTK_STACK(priv->browse_stack)); */

            /* if (view == priv->channels_view) */
            /*     gt_channels_view_handle_event(GT_CHANNELS_VIEW(priv->channels_view), (GdkEvent*) evt); */
            /* else if (view == priv->games_view) */
            /*     gt_games_view_handle_event(GT_GAMES_VIEW(priv->games_view), (GdkEvent*) evt); */
            /* else if (view == priv->follows_view) */
            /*     gt_follows_view_handle_event(GT_FOLLOWS_VIEW(priv->follows_view), (GdkEvent* )evt); */
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
close_channel_cb(GSimpleAction* action,
    GVariant* arg, gpointer udata)
{
    RETURN_IF_FAIL(G_IS_SIMPLE_ACTION(action));
    RETURN_IF_FAIL(arg == NULL);
    RETURN_IF_FAIL(GT_IS_WIN(udata));

    GtWin* self = GT_WIN(udata);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    gt_player_close_channel(GT_PLAYER(self->player));

    gtk_stack_set_visible_child(GTK_STACK(priv->main_stack),
        priv->browse_stack);
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

static gboolean
window_state_event_cb(GtkWidget* widget,
    GdkEventWindowState* evt, gpointer udata)
{
    RETURN_VAL_IF_FAIL(GT_IS_WIN(widget), GDK_EVENT_PROPAGATE);
    RETURN_VAL_IF_FAIL(udata == NULL, GDK_EVENT_PROPAGATE);

    GtWin* self = GT_WIN(widget);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    priv->fullscreen = evt->new_window_state & GDK_WINDOW_STATE_FULLSCREEN;

    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_FULLSCREEN]);

    return GDK_EVENT_PROPAGATE;
}

static void
update_fullscreen(GtWin* self, gboolean fullscreen)
{
    if (fullscreen)
        gtk_window_fullscreen(GTK_WINDOW(self));
    else
        gtk_window_unfullscreen(GTK_WINDOW(self));
}

static GActionEntry win_actions[] =
{
    {"refresh", refresh_cb, NULL, NULL, NULL},
    {"go_back", go_back_cb, NULL, NULL, NULL},
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
        case PROP_FULLSCREEN:
            g_value_set_boolean(val, priv->fullscreen);
            break;
        case PROP_OPEN_CHANNEL:
            g_value_set_object(val, self->open_channel);
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

    switch (prop)
    {
        case PROP_FULLSCREEN:
            update_fullscreen(self, g_value_get_boolean(val));
            break;
        case PROP_OPEN_CHANNEL:
            g_clear_object(&self->open_channel);
            self->open_channel = g_value_dup_object(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
constructed(GObject* obj)
{
    GtWin* self = GT_WIN(obj);
    GtWinPrivate* priv = gt_win_get_instance_private(self);

    g_signal_connect(priv->browse_stack, "notify::visible-child",
        G_CALLBACK(container_view_changed_cb), self);

    container_view_changed_cb(NULL, NULL, self);

    G_OBJECT_CLASS(gt_win_parent_class)->constructed(obj);
}

static void
gt_win_class_init(GtWinClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;
    object_class->constructed = constructed;

    props[PROP_FULLSCREEN] = g_param_spec_boolean("fullscreen", "Fullscreen", "Whether window is fullscreen",
        FALSE, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

    props[PROP_OPEN_CHANNEL] = g_param_spec_object("open-channel", "Open channel", "Currenly open channel",
        GT_TYPE_CHANNEL, G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, NUM_PROPS, props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
                                                "/com/vinszent/GnomeTwitch/ui/gt-win.ui");
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, main_stack);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), GtWin, player);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, header_stack);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, player_header_bar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, browse_header_bar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, browse_stack);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, browse_stack_switcher);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, info_revealer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, info_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, info_bar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, info_bar_yes_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, info_bar_no_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, info_bar_ok_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, info_bar_details_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, info_bar_close_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, channel_stack);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, channel_vod_container);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtWin, channel_header_bar);

    GT_TYPE_PLAYER; // Hack to load GtPlayer into the symbols table
    GT_TYPE_PLAYER_HEADER_BAR;
    GT_TYPE_BROWSE_HEADER_BAR;
    GT_TYPE_CHANNEL_CONTAINER_VIEW;
    GT_TYPE_FOLLOWED_CONTAINER_VIEW;
    GT_TYPE_GAME_CONTAINER_VIEW;
    GT_TYPE_CHANNEL_VOD_CONTAINER;
    GT_TYPE_CHANNEL_HEADER_BAR;
    GT_TYPE_CHAT;
}

static void
gt_win_init(GtWin* self)
{
    GtWinPrivate* priv = gt_win_get_instance_private(self);
    GPropertyAction* action;

    gtk_window_set_application(GTK_WINDOW(self), GTK_APPLICATION(main_app));

    gtk_widget_init_template(GTK_WIDGET(self));
    gtk_widget_realize(priv->player_header_bar);

    priv->cur_info_data = NULL;
    priv->info_queue = g_queue_new();

    gtk_window_set_default_size(GTK_WINDOW(self),
                                g_settings_get_int(main_app->settings, "window-width"),
                                g_settings_get_int(main_app->settings, "window-height"));

    gtk_window_set_default_icon_name("com.vinszent.GnomeTwitch");

    gtk_window_set_icon_name(GTK_WINDOW(self), "com.vinszent.GnomeTwitch");

    GdkScreen* screen = gdk_screen_get_default();
    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(css, "/com/vinszent/GnomeTwitch/com.vinszent.GnomeTwitch.style.css");
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(css),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_signal_connect_after(self, "key-press-event", G_CALLBACK(key_press_cb), self);
    g_signal_connect(self, "delete-event", G_CALLBACK(delete_cb), self);
    g_signal_connect_after(priv->info_bar, "response", G_CALLBACK(close_info_bar_cb), self);
    g_signal_connect(self, "window-state-event", G_CALLBACK(window_state_event_cb), NULL);

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

void
gt_win_open_channel(GtWin* self, GtChannel* chan)
{
    g_assert(GT_IS_WIN(self));
    g_assert(GT_IS_CHANNEL(chan));

    GtWinPrivate* priv = gt_win_get_instance_private(self);

    /* if (gt_player_is_playing(self->player)) */
    /*     gt_player_close_channel(self->player); */

    if (gt_channel_is_online(chan))
    {
        g_object_set(self, "open-channel", chan, NULL);

        gt_player_open_channel(GT_PLAYER(self->player), chan);

        g_object_set(G_OBJECT(priv->channel_vod_container), "channel-id", gt_channel_get_id(chan), NULL);

        gt_item_container_refresh(GT_ITEM_CONTAINER(priv->channel_vod_container));

        gtk_stack_set_visible_child(GTK_STACK(priv->main_stack), priv->channel_stack);
    }
    else
    {
        gt_win_show_info_message(self, _("Unable to open channel %s because it’s not online"),
            gt_channel_get_name(chan));

        gtk_stack_set_visible_child_name(GTK_STACK(priv->main_stack),
            "browse");
        gtk_stack_set_visible_child_name(GTK_STACK(priv->header_stack),
            "browse");
    }
}

void
gt_win_play_vod(GtWin* self, GtVOD* vod)
{
    RETURN_IF_FAIL(GT_IS_WIN(self));
    RETURN_IF_FAIL(GT_IS_VOD(vod));

    GtWinPrivate* priv = gt_win_get_instance_private(self);

    gt_player_open_vod(self->player, vod);
    gtk_stack_set_visible_child(GTK_STACK(priv->channel_stack), GTK_WIDGET(self->player));
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
gt_win_show_info_message(GtWin* self, const gchar* msg_fmt, ...)
{
    RETURN_IF_FAIL(GT_IS_WIN(self));

    GtWinPrivate* priv = gt_win_get_instance_private(self);
    QueuedInfoData* data = g_new0(QueuedInfoData, 1);
    va_list fmt_list;

    va_start(fmt_list, msg_fmt);
    data->msg = g_strdup_vprintf(msg_fmt, fmt_list);
    va_end(fmt_list);

    g_queue_push_tail(priv->info_queue, data);

    if (!gtk_revealer_get_reveal_child(GTK_REVEALER(priv->info_revealer)))
        show_info_bar(self);
}

void
gt_win_show_error_message(GtWin* self, const gchar* secondary, const gchar* details_fmt, ...)
{
    g_assert(GT_IS_WIN(self));

    gchar** udata = g_malloc(sizeof(gchar*)*3);
    // Translators: Please keep the markup tags
    gchar* msg = g_strdup_printf(_("<b>Something went wrong:</b> %s."), secondary);
    va_list details_list;

    *udata = g_strdup(secondary);

    va_start(details_list, details_fmt);
    *(udata+1) = g_strdup_vprintf(details_fmt, details_list);
    va_end(details_list);
    *(udata+2) = NULL;

    gt_win_ask_question(self, msg, G_CALLBACK(show_error_dialogue_cb), udata);
}

void
gt_win_show_error_dialogue(GtWin* self, const gchar* secondary, const gchar* details_fmt, ...)
{
    g_assert(GT_IS_WIN(self));

    gchar** udata = g_malloc(sizeof(gchar*)*3);
    va_list details_list;

    *udata = g_strdup(secondary);

    va_start(details_list, details_fmt);
    *(udata+1) = g_strdup_vprintf(details_fmt, details_list);
    va_end(details_list);
    *(udata+2) = NULL;

    show_error_dialogue_cb(NULL, GTK_RESPONSE_OK, udata);
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
