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

#include "gt-app.h"
#include "gt-win.h"
#include "gt-http-soup.h"
#include "config.h"
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <string.h>
#include <json-glib/json-glib.h>
#include <stdlib.h>
#include "utils.h"
#include "version.h"

#define TAG "GtApp"
#include "gnome-twitch/gt-log.h"

struct _GtAppPrivate
{
    GtWin* win;

    GtUserInfo* user_info;
    GtOAuthInfo* oauth_info;
    GMenuItem* login_item;
    GMenuModel* app_menu;

    gchar* language_filter;

    GCancellable* open_channel_cancel;
    GHashTable* soup_inflight_table;
    GQueue* soup_message_queue;
};

gint LOG_LEVEL = GT_LOG_LEVEL_MESSAGE;
gboolean NO_FANCY_LOGGING = FALSE;
gboolean VERSION = FALSE;

const gchar* TWITCH_AUTH_SCOPES[] =
{
    "chat_login",
    "user_follows_edit",
    "user_read",
    NULL
};


G_DEFINE_TYPE_WITH_PRIVATE(GtApp, gt_app, GTK_TYPE_APPLICATION)

enum
{
    PROP_0,
    PROP_LOGGED_IN,
    PROP_LANGUAGE_FILTER,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

static gboolean
set_log_level(const gchar* name,
              const gchar* arg,
              gpointer data,
              GError** err)
{
    if (g_strcmp0(arg, "error") == 0)
        LOG_LEVEL = GT_LOG_LEVEL_ERROR;
    else if (g_strcmp0(arg, "critical") == 0)
        LOG_LEVEL = GT_LOG_LEVEL_CRITICAL;
    else if (g_strcmp0(arg, "warning") == 0)
        LOG_LEVEL = GT_LOG_LEVEL_WARNING;
    else if (g_strcmp0(arg, "message") == 0)
        LOG_LEVEL = GT_LOG_LEVEL_MESSAGE;
    else if (g_strcmp0(arg, "info") == 0)
        LOG_LEVEL = GT_LOG_LEVEL_INFO;
    else if (g_strcmp0(arg, "debug") == 0)
        LOG_LEVEL = GT_LOG_LEVEL_DEBUG;
    else if (g_strcmp0(arg, "trace") == 0)
        LOG_LEVEL = GT_LOG_LEVEL_TRACE;
    else
        WARNING("Invalid logging level"); //TODO: Use g_set_error and return false

    return TRUE;
}

static GOptionEntry
cli_options[] =
{
    {"log-level", 'l', G_OPTION_FLAG_NONE, G_OPTION_ARG_CALLBACK, set_log_level, "Set logging level", "level"},
    {"no-fancy-logging", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &NO_FANCY_LOGGING, "Don't print pretty log messages", NULL},
    {"version", 'v', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &VERSION, "Display version", NULL},
    {NULL}
};

GtApp*
gt_app_new(void)
{
    return g_object_new(GT_TYPE_APP,
                        "application-id", "com.vinszent.GnomeTwitch",
                        NULL);
}

static void
oauth_info_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    g_assert(GT_IS_APP(udata));
    g_assert(GT_IS_TWITCH(source));

    GtApp* self = GT_APP(udata);
    GError* err = NULL;

    GtOAuthInfo* oauth_info = gt_twitch_fetch_oauth_info_finish(GT_TWITCH(source), res, &err);

    if (err)
    {
        WARNING("Unable to update oauth info because: %s", err->message);

        GtWin* win = GT_WIN_ACTIVE;

        g_assert(GT_IS_WIN(win));

        gt_win_show_error_message(win, "Unable to update oauth info",
            "Unable to update oauth info because: %s", err->message);

        g_error_free(err);

        return;
    }

    gt_app_set_oauth_info(self, oauth_info);

    MESSAGE("Successfully fetched oauth info with name '%s', id '%s' and oauth token '%s'",
        oauth_info->user_name, oauth_info->user_id, oauth_info->oauth_token);
}

static void
user_info_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    g_assert(GT_IS_APP(udata));
    g_assert(GT_IS_TWITCH(source));

    GtApp* self = GT_APP(udata);
    GtAppPrivate* priv = gt_app_get_instance_private(self);
    GError* err = NULL;

    GtUserInfo* user_info = gt_twitch_fetch_user_info_finish(GT_TWITCH(source), res, &err);

    if (err)
    {
        WARNING("Unable to update user info because: %s", err->message);

        GtWin* win = GT_WIN_ACTIVE;

        g_assert(GT_IS_WIN(win));

        gt_win_show_error_message(win, "Unable to update user info",
            "Unable to update user info because: %s", err->message);

        g_error_free(err);

        return;
    }

    priv->user_info = user_info;

    MESSAGE("Successfully fetched user info with name '%s', id '%s' and oauth token '%s'",
        user_info->name, user_info->id, user_info->oauth_token);
}

static void
logged_in_cb(GObject* src,
    GParamSpec* pspec, gpointer udata)
{
    g_assert(GT_IS_APP(src));

    GtApp* self = GT_APP(src);
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    if (gt_app_is_logged_in(self))
    {
        g_menu_remove(G_MENU(priv->app_menu), 0);
        g_menu_item_set_label(priv->login_item, _("Refresh login"));
        g_menu_prepend_item(G_MENU(priv->app_menu), priv->login_item);
    }
}

/* FIXME: Move this into GtResourceDownloader */
static inline void
init_dirs()
{
    gchar* fp;
    int err;

    fp = g_build_filename(g_get_user_data_dir(), "gnome-twitch", NULL);
    err = g_mkdir_with_parents(fp, 0777);
    if (err != 0 && g_file_error_from_errno(errno) != G_FILE_ERROR_EXIST)
        WARNING("Error creating data directory");
    g_free(fp);

    fp = g_build_filename(g_get_user_cache_dir(), "gnome-twitch", "channels", NULL);
    err = g_mkdir_with_parents(fp, 0777);
    if (err != 0 && g_file_error_from_errno(errno) != G_FILE_ERROR_EXIST)
        WARNING("Error creating channel cache directory");
    g_free(fp);

    fp = g_build_filename(g_get_user_cache_dir(), "gnome-twitch", "games", NULL);
    err = g_mkdir_with_parents(fp, 0777);
    if (err != 0 && g_file_error_from_errno(errno) != G_FILE_ERROR_EXIST)
        WARNING("Error creating game cache directory");
    g_free(fp);

    fp = g_build_filename(g_get_user_cache_dir(), "gnome-twitch", "emotes", NULL);
    err = g_mkdir_with_parents(fp, 0777);
    if (err != 0 && g_file_error_from_errno(errno) != G_FILE_ERROR_EXIST)
        WARNING("Error creating game cache directory");
    g_free(fp);

    fp = g_build_filename(g_get_user_cache_dir(), "gnome-twitch", "badges", NULL);
    err = g_mkdir_with_parents(fp, 0777);
    if (err != 0 && g_file_error_from_errno(errno) != G_FILE_ERROR_EXIST)
        WARNING("Error creating game cache directory");
    g_free(fp);
}

static void
open_channel_after_fetch_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    g_assert(GT_IS_APP(udata));
    g_assert(GT_IS_TWITCH(source));
    g_assert(G_IS_ASYNC_RESULT(res));

    GtApp* self = udata;
    g_autoptr(GError) err = NULL;
    //NOTE: autoptr because we want this unrefed as we won't be using it anymore
    g_autoptr(GtChannel) channel = NULL;
    GtWin* win = NULL;

    channel = gt_twitch_fetch_channel_finish(GT_TWITCH(source),
        res, &err);

    win = GT_WIN_ACTIVE;

    g_assert(GT_IS_WIN(win));

    if (err)
    {
        WARNING("Couldn't open channel because: %s", err->message);

        gt_win_show_error_message(win, "Couldn't open channel",
            "Couldn't open channel because: %s", err->message);

    }
    else if (!gt_channel_is_online(channel))
    {
        gt_win_show_info_message(win, _("Unable to open channel %s because it’s not online"),
            gt_channel_get_name(channel));
    }
    else
        gt_win_open_channel(win, channel); //NOTE: Win will take a reference to channel

    g_object_unref(self);
}

static void
open_channel_from_id_cb(GSimpleAction* action,
    GVariant* var, gpointer udata)
{
    g_assert(G_IS_SIMPLE_ACTION(action));
    g_assert(GT_IS_APP(udata));
    g_assert(g_variant_is_of_type(var, G_VARIANT_TYPE_STRING));

    GtApp* self = GT_APP(udata);
    GtAppPrivate* priv = gt_app_get_instance_private(self);
    const gchar* id = g_variant_get_string(var, NULL);

    utils_refresh_cancellable(&priv->open_channel_cancel);

    MESSAGE("Opening channel from id '%s'", id);

    gt_twitch_fetch_channel_async(self->twitch, id,
        open_channel_after_fetch_cb, priv->open_channel_cancel,
        g_object_ref(self));
}

static void
quit_cb(GSimpleAction* action,
    GVariant* par, gpointer udata)
{
    MESSAGE("Quitting");

    GtApp* self = GT_APP(udata);

    g_application_quit(G_APPLICATION(self));
}

static gint
handle_command_line_cb(GApplication* self,
    GVariantDict* options, gpointer udata)
{
    if (VERSION)
    {
        g_print("GNOME Twitch - Version %s\n", GT_VERSION);
        return 0;
    }

    return -1;
}

static GActionEntry app_actions[] =
{
    {"open-channel-from-id", open_channel_from_id_cb, "s", NULL, NULL},
    {"quit", quit_cb, NULL, NULL, NULL}
};

static void
activate(GApplication* app)
{
    GtApp* self = GT_APP(app);
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    G_APPLICATION_CLASS(gt_app_parent_class)->activate(app);

    MESSAGE("Activate");

    priv->win = gt_win_new(self);

    gtk_window_present(GTK_WINDOW(priv->win));
}

static void
gt_app_prefer_dark_theme_changed_cb(GSettings* settings,
                                    const char* key,
                                    GtkSettings* gtk_settings)
{
    gboolean prefer_dark_theme = g_settings_get_boolean(settings, key);

    g_object_set(gtk_settings,
                 "gtk-application-prefer-dark-theme",
                 prefer_dark_theme,
                 NULL);
}

//Only called once
static void
startup(GApplication* app)
{
    GtApp* self = GT_APP(app);
    GtAppPrivate* priv = gt_app_get_instance_private(self);
    GtkSettings* gtk_settings;
    g_autoptr(GtkBuilder) menu_bld;

    G_APPLICATION_CLASS(gt_app_parent_class)->startup(app);

    MESSAGE("Startup, running version '%s'", GT_VERSION);

    self->fav_mgr = gt_follows_manager_new();
    self->twitch = gt_twitch_new();

    init_dirs();

    g_action_map_add_action_entries(G_ACTION_MAP(self),
        app_actions, G_N_ELEMENTS(app_actions), self);

    menu_bld = gtk_builder_new_from_resource("/com/vinszent/GnomeTwitch/ui/app-menu.ui");
    priv->app_menu = G_MENU_MODEL(gtk_builder_get_object(menu_bld, "app_menu"));
    g_object_ref(priv->app_menu);
    priv->login_item = g_menu_item_new(_("Log in to Twitch"), "win.show_twitch_login");
    g_menu_prepend_item(G_MENU(priv->app_menu), priv->login_item);

    gtk_application_set_app_menu(GTK_APPLICATION(app), G_MENU_MODEL(priv->app_menu));

    gtk_settings = gtk_settings_get_default();

    gt_app_prefer_dark_theme_changed_cb(self->settings,
        "prefer-dark-theme", gtk_settings);

    g_signal_connect(self->settings, "changed::prefer-dark-theme",
        G_CALLBACK(gt_app_prefer_dark_theme_changed_cb), gtk_settings);

    //NOTE: This is to refresh oauth info
    if (gt_app_is_logged_in(self))
    {
        gt_twitch_fetch_oauth_info_async(self->twitch, priv->oauth_info->oauth_token,
            oauth_info_cb, NULL, self);
    }
    else
        g_object_notify_by_pspec(G_OBJECT(self), props[PROP_LOGGED_IN]);
}

static void
shutdown(GApplication* app)
{
    GtApp* self = GT_APP(app);

    MESSAGE("Shutting down");

    //TODO: Add a setting to allow user to use local follows even when logged in
    if (!gt_app_is_logged_in(self))
        gt_follows_manager_save(self->fav_mgr);

    G_APPLICATION_CLASS(gt_app_parent_class)->shutdown(app);
}

static void
dispose(GObject* object)
{
    g_assert(GT_IS_APP(object));

    GtApp* self = GT_APP(object);
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    g_hash_table_unref(priv->soup_inflight_table);
    g_queue_free_full(priv->soup_message_queue, g_object_unref);
    g_object_unref(self->http);

    G_OBJECT_CLASS(gt_app_parent_class)->dispose(object);
}

static void
get_property (GObject* obj, guint prop,
    GValue* val, GParamSpec* pspec)
{
    GtApp* self = GT_APP(obj);
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    switch (prop)
    {
        case PROP_LOGGED_IN:
            g_value_set_boolean(val, priv->oauth_info != NULL);
            break;
        case PROP_LANGUAGE_FILTER:
            g_value_set_string(val, priv->language_filter);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
set_property(GObject* obj, guint prop,
    const GValue* val, GParamSpec* pspec)
{
    GtApp* self = GT_APP(obj);
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    switch (prop)
    {
        case PROP_LANGUAGE_FILTER:
            g_free(priv->language_filter);
            priv->language_filter = g_value_dup_string(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_app_class_init(GtAppClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    G_OBJECT_CLASS(klass)->dispose = dispose;

    G_APPLICATION_CLASS(klass)->activate = activate;
    G_APPLICATION_CLASS(klass)->startup = startup;
    G_APPLICATION_CLASS(klass)->shutdown = shutdown;

    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_LOGGED_IN] = g_param_spec_boolean("logged-in",
        "Logged in", "Whether logged in", FALSE, G_PARAM_READABLE);

    props[PROP_LANGUAGE_FILTER] = g_param_spec_string("language-filter",
        "Language filter", "Current language filter", "", G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, NUM_PROPS, props);
}

static void
gt_app_init(GtApp* self)
{
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    priv->oauth_info = gt_oauth_info_new();
    priv->soup_inflight_table = g_hash_table_new(g_str_hash, g_str_equal);
    priv->soup_message_queue = g_queue_new();

    g_application_add_main_option_entries(G_APPLICATION(self), cli_options);

    self->settings = g_settings_new("com.vinszent.GnomeTwitch");
    self->players_engine = peas_engine_get_default();
    peas_engine_enable_loader(self->players_engine, "python3");
    self->soup = soup_session_new();
    self->http = GT_HTTP(gt_http_soup_new());

    gchar* plugin_dir;

#ifdef G_OS_WIN32
    plugin_dir = g_build_filename(
        g_win32_get_package_installation_directory_of_module(NULL),
        "lib", "gnome-twitch", "player-backends", NULL);

    peas_engine_add_search_path(self->players_engine, plugin_dir, NULL);

    g_free(plugin_dir);
#else
    plugin_dir = g_build_filename(GT_LIB_DIR, "gnome-twitch",
        "player-backends", NULL);

    peas_engine_add_search_path(self->players_engine, plugin_dir, NULL);

    g_free(plugin_dir);

    plugin_dir = g_build_filename(g_get_user_data_dir(), "gnome-twitch",
        "player-backends", NULL);

    peas_engine_add_search_path(self->players_engine, plugin_dir, NULL);

    g_free(plugin_dir);
#endif

    g_settings_bind(self->settings, "player-backend",
        self->players_engine, "loaded-plugins",
        G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(self->settings, "language-filter",
        self, "language-filter",
        G_SETTINGS_BIND_GET);

    priv->oauth_info->oauth_token = g_settings_get_string(self->settings, "oauth-token");
    priv->oauth_info->user_id = g_settings_get_string(self->settings, "user-id");
    priv->oauth_info->user_name = g_settings_get_string(self->settings, "user-name");

    g_signal_connect(self, "notify::logged-in", G_CALLBACK(logged_in_cb), self);
    g_signal_connect(self, "handle-local-options", G_CALLBACK(handle_command_line_cb), self);

}

//TODO: Turn this into a property
void
gt_app_set_oauth_info(GtApp* self, GtOAuthInfo* info)
{
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    if (priv->oauth_info)
        gt_oauth_info_free(priv->oauth_info);

    priv->oauth_info = info;

    for (gchar** s = (gchar**) TWITCH_AUTH_SCOPES; *s != NULL; s++)
    {
        if (!g_strv_contains((const gchar**) info->scopes, *s))
        {
            GtWin* win = GT_WIN_ACTIVE;

            g_assert(GT_IS_WIN(win));

            WARNING("Unable to update oauth info because the auth scope '%s' was not included", *s);

            gt_win_show_error_message(win, "Unable to update oauth info, try refreshing your login",
                "Unable to update oauth info because the auth scope '%s' was not included", *s);

            g_object_notify_by_pspec(G_OBJECT(self), props[PROP_LOGGED_IN]);

            return;
        }
    }

    g_settings_set_string(self->settings, "oauth-token", info->oauth_token);
    g_settings_set_string(self->settings, "user-id", info->user_id);
    g_settings_set_string(self->settings, "user-name", info->user_name);

    gt_twitch_fetch_user_info_async(self->twitch, priv->oauth_info->oauth_token,
        user_info_cb, NULL, self);

    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_LOGGED_IN]);
}

const GtUserInfo*
gt_app_get_user_info(GtApp* self)
{
    g_assert(GT_IS_APP(self));

    GtAppPrivate* priv = gt_app_get_instance_private(self);

    return priv->user_info;
}

const GtOAuthInfo*
gt_app_get_oauth_info(GtApp* self)
{
    g_assert(GT_IS_APP(self));

    GtAppPrivate* priv = gt_app_get_instance_private(self);

    return priv->oauth_info;
}

gboolean
gt_app_is_logged_in(GtApp* self)
{
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    return
        priv->oauth_info &&
        !utils_str_empty(priv->oauth_info->oauth_token) &&
        !utils_str_empty(priv->oauth_info->user_name) &&
        !utils_str_empty(priv->oauth_info->user_id);
}

const gchar*
gt_app_get_language_filter(GtApp* self)
{
    g_assert(GT_IS_APP(self));

    GtAppPrivate* priv = gt_app_get_instance_private(self);

    return priv->language_filter;
}

gboolean
gt_app_should_show_notifications(GtApp* self)
{
    g_assert(GT_IS_APP(self));

    return g_settings_get_boolean(main_app->settings, "show-notifications");
}

GtUserInfo*
gt_user_info_new()
{
    return g_slice_new0(GtUserInfo);
}

void
gt_user_info_free(GtUserInfo* info)
{
    if (!info) return;

    g_free(info->name);
    g_free(info->oauth_token);
    g_free(info->display_name);
    g_free(info->bio);
    g_free(info->logo_url);
    g_free(info->type);
    g_free(info->email);

    if (info->created_at) g_date_time_unref(info->created_at);
    if (info->updated_at) g_date_time_unref(info->updated_at);

    g_slice_free(GtUserInfo, info);
}

GtOAuthInfo*
gt_oauth_info_new()
{
    return g_slice_new0(GtOAuthInfo);
}

void
gt_oauth_info_free(GtOAuthInfo* info)
{
    if (!info) return;

    g_free(info->user_name);
    g_free(info->user_id);
    g_free(info->client_id);
    g_strfreev(info->scopes);

    if (info->created_at) g_date_time_unref(info->created_at);
    if (info->updated_at) g_date_time_unref(info->updated_at);

    g_slice_free(GtOAuthInfo, info);
}
