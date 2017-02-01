#include "gt-app.h"
#include "gt-win.h"
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

#define CHANNEL_SETTINGS_FILE g_build_filename(g_get_user_data_dir(), "gnome-twitch", "channel_settings.json", NULL);

typedef struct
{
    GtWin* win;

    GtUserInfo* user_info;
    GMenuItem* login_item;
    GMenuModel* app_menu;
} GtAppPrivate;

gint LOG_LEVEL = GT_LOG_LEVEL_MESSAGE;
gboolean NO_FANCY_LOGGING = FALSE;
gboolean VERSION = FALSE;

G_DEFINE_TYPE_WITH_PRIVATE(GtApp, gt_app, GTK_TYPE_APPLICATION)

enum
{
    PROP_0,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

enum
{
    NUM_SIGS
};

static guint sigs[NUM_SIGS];

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

GtChatViewSettings*
gt_chat_view_settings_new()
{
    GtChatViewSettings* ret = g_new0(GtChatViewSettings, 1);

    ret->dark_theme = TRUE;
    ret->opacity = 1.0;
    ret->visible = TRUE;
    ret->docked = TRUE;
    ret->width = 0.2;
    ret->height = 1.0;
    ret->x_pos = 0;
    ret->y_pos = 0;
    ret->docked_handle_pos = 0.75;

    return ret;
}

static void
load_chat_settings(GtApp* self)
{
    gchar* fp = CHANNEL_SETTINGS_FILE;
    JsonParser* parse = json_parser_new();
    JsonNode* root = NULL;
    JsonArray* array = NULL;
    GList* ele = NULL;
    GError* err = NULL;

    MESSAGE("Loading chat settings");

    if (!g_file_test(fp, G_FILE_TEST_EXISTS))
        goto finish;

    json_parser_load_from_file(parse, fp, &err);

    if (err)
    {
        WARNINGF("Error loading chat settings '%s'", err->message);
        goto finish;
    }

    root = json_parser_get_root(parse);
    array = json_node_get_array(root);
    ele = json_array_get_elements(array);

    for (GList* l = ele; l != NULL; l = l->next)
    {
        JsonNode* node = l->data;
        JsonObject* chan = json_node_get_object(node);
        GtChatViewSettings* settings = gt_chat_view_settings_new();
        const gchar* name;

        name = json_object_get_string_member(chan, "name");
        settings->dark_theme = json_object_get_boolean_member(chan, "dark-theme");
        settings->visible = json_object_get_boolean_member(chan, "visible");
        settings->docked = json_object_get_boolean_member(chan, "docked");
        settings->opacity = json_object_get_double_member(chan, "opacity");
        settings->width = json_object_get_double_member(chan, "width");
        settings->height = json_object_get_double_member(chan, "height");
        settings->x_pos = json_object_get_double_member(chan, "x-pos");
        settings->y_pos = json_object_get_double_member(chan, "y-pos");
        if (json_object_has_member(chan, "docked-handle-pos"))
            settings->docked_handle_pos = json_object_get_double_member(chan, "docked-handle-pos");
        else
            settings->docked_handle_pos = 0.75;

        g_hash_table_insert(self->chat_settings_table, g_strdup(name), settings);
    }

    g_list_free(ele);

finish:
    g_object_unref(parse);
    g_free(fp);
}

static void
save_chat_settings(GtApp* self)
{
    gchar* fp = CHANNEL_SETTINGS_FILE;
    JsonArray* array = json_array_new();
    JsonGenerator* generator = json_generator_new();
    JsonNode* root = json_node_new(JSON_NODE_ARRAY);
    GList* keys = g_hash_table_get_keys(self->chat_settings_table);
    GError* err = NULL;

    MESSAGE("{GtApp} Saving chat settings");

    for (GList* l = keys; l != NULL; l = l->next)
    {
        JsonObject* obj = json_object_new();
        JsonNode* node = json_node_new(JSON_NODE_OBJECT);
        const gchar* key = l->data;
        GtChatViewSettings* settings = g_hash_table_lookup(self->chat_settings_table, key);

        json_object_set_string_member(obj, "name", key);
        json_object_set_boolean_member(obj, "dark-theme", settings->dark_theme);
        json_object_set_boolean_member(obj, "visible", settings->visible);
        json_object_set_boolean_member(obj, "docked", settings->docked);
        json_object_set_double_member(obj, "opacity", settings->opacity);
        json_object_set_double_member(obj, "width", settings->width);
        json_object_set_double_member(obj, "height", settings->height);
        json_object_set_double_member(obj, "x-pos", settings->x_pos);
        json_object_set_double_member(obj, "y-pos", settings->y_pos);
        json_object_set_double_member(obj, "docked-handle-pos", settings->docked_handle_pos);

        json_node_take_object(node, obj);
        json_array_add_element(array, node);
    }

    json_node_take_array(root, array);

    json_generator_set_root(generator, root);
    json_generator_to_file(generator, fp, &err);

    if (err)
        WARNINGF("Error saving chat settings '%s'", err->message);

    json_node_free(root);
    g_object_unref(generator);
    g_list_free(keys);
    g_free(fp);
}

static void
oauth_token_set_cb(GObject* src,
                   GParamSpec* pspec,
                   gpointer udata)
{
    GtApp* self = GT_APP(udata);
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    if (!utils_str_empty(priv->user_info->oauth_token))
    {
        g_menu_remove(G_MENU(priv->app_menu), 0);
        g_menu_item_set_label(priv->login_item, _("Refresh login"));
        g_menu_prepend_item(G_MENU(priv->app_menu), priv->login_item);
    }
}

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
}


static void
quit_cb(GSimpleAction* action,
        GVariant* par,
        gpointer udata)
{
    MESSAGE("Quitting");

    GtApp* self = GT_APP(udata);

    g_application_quit(G_APPLICATION(self));
}

static gint
handle_command_line_cb(GApplication* self,
                       GVariantDict* options,
                       gpointer udata)
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
    GtkBuilder* menu_bld;

    MESSAGE("Startup");

    G_APPLICATION_CLASS(gt_app_parent_class)->startup(app);

    load_chat_settings(self);
    init_dirs();

    g_action_map_add_action_entries(G_ACTION_MAP(self),
                                    app_actions,
                                    G_N_ELEMENTS(app_actions),
                                    self);

    menu_bld = gtk_builder_new_from_resource("/com/vinszent/GnomeTwitch/ui/app-menu.ui");
    priv->app_menu = G_MENU_MODEL(gtk_builder_get_object(menu_bld, "app_menu"));
    g_object_ref(priv->app_menu);
    priv->login_item = g_menu_item_new(_("Login to Twitch"), "win.show_twitch_login");
    g_menu_prepend_item(G_MENU(priv->app_menu), priv->login_item);

    gtk_application_set_app_menu(GTK_APPLICATION(app), G_MENU_MODEL(priv->app_menu));

    g_object_unref(menu_bld);

    /* g_settings_bind(self->settings, "user-name", */
    /*                 self, "user-name", */
    /*                 G_SETTINGS_BIND_DEFAULT); */
    /* g_settings_bind(self->settings, "oauth-token", */
    /*                 self, "oauth-token", */
    /*                 G_SETTINGS_BIND_DEFAULT); */

    gtk_settings = gtk_settings_get_default();

    gt_app_prefer_dark_theme_changed_cb(self->settings,
                                        "prefer-dark-theme",
                                        gtk_settings);
    g_signal_connect(self->settings,
                     "changed::prefer-dark-theme",
                     G_CALLBACK(gt_app_prefer_dark_theme_changed_cb),
                     gtk_settings);

    self->fav_mgr = gt_follows_manager_new();

    //TODO: Add a setting to allow user to use local follows even when logged in
    if (gt_app_credentials_valid(self))
        gt_follows_manager_load_from_twitch(self->fav_mgr);
    else
        gt_follows_manager_load_from_file(self->fav_mgr);
}

static void
shutdown(GApplication* app)
{

    GtApp* self = GT_APP(app);

    MESSAGE("{GtApp} Shutting down");

    save_chat_settings(self);

    //TODO: Add a setting to allow user to use local follows even when logged in
    if (!gt_app_credentials_valid(self))
        gt_follows_manager_save(self->fav_mgr);

    G_APPLICATION_CLASS(gt_app_parent_class)->shutdown(app);
}


static void
finalize(GObject* object)
{
    GtApp* self = (GtApp*) object;
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    DEBUG("{GtApp} Finalise");

    G_OBJECT_CLASS(gt_app_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtApp* self = GT_APP(obj);
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    switch (prop)
    {
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
    GtApp* self = GT_APP(obj);
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_app_class_init(GtAppClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    G_APPLICATION_CLASS(klass)->activate = activate;
    G_APPLICATION_CLASS(klass)->startup = startup;
    G_APPLICATION_CLASS(klass)->shutdown = shutdown;

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;
}

static void
gt_app_init(GtApp* self)
{
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    priv->user_info = NULL;

    g_application_add_main_option_entries(G_APPLICATION(self), cli_options);

    self->twitch = gt_twitch_new();
    self->settings = g_settings_new("com.vinszent.GnomeTwitch");
    self->chat_settings_table = g_hash_table_new(g_str_hash, g_str_equal);
    self->players_engine = peas_engine_get_default();

    gchar* plugin_dir;

#define ADD_PLUGINS_PATH(root, type)                                    \
    plugin_dir = g_build_filename(root,                                 \
                                  "gnome-twitch",                       \
                                  "plugins",                            \
                                  type,                                 \
                                  NULL);                                \
    peas_engine_add_search_path(self->players_engine, plugin_dir, NULL); \
    g_free(plugin_dir)                                                  \

    ADD_PLUGINS_PATH(GT_LIB_DIR, "player-backends");
    ADD_PLUGINS_PATH(g_get_user_data_dir(), "player-backends");
//    ADD_PLUGINS_PATH(GT_LIB_DIR, "recorder-backends");
//    ADD_PLUGINS_PATH(g_get_user_data_dir(), "recorder-backends");

#undef ADD_PLUGINS_PATH

    g_settings_bind(self->settings, "loaded-plugins",
                    self->players_engine, "loaded-plugins",
                    G_SETTINGS_BIND_DEFAULT);

    g_signal_connect(self, "notify::oauth-token", G_CALLBACK(oauth_token_set_cb), self);
    g_signal_connect(self, "handle-local-options", G_CALLBACK(handle_command_line_cb), self);
}

//TODO: Turn this into a property
void
gt_app_set_user_info(GtApp* self, GtUserInfo* info)
{
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    if (priv->user_info)
        gt_user_info_free(priv->user_info);

    priv->user_info = info;
}

const GtUserInfo*
gt_app_get_user_info(GtApp* self)
{
    g_assert(GT_IS_APP(self));

    GtAppPrivate* priv = gt_app_get_instance_private(self);

    return priv->user_info;
}

G_DEPRECATED
const gchar*
gt_app_get_user_name(GtApp* self)
{
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    return priv->user_info->name;
}

G_DEPRECATED
const gchar*
gt_app_get_oauth_token(GtApp* self)
{
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    return priv->user_info->oauth_token;
}

gboolean
gt_app_credentials_valid(GtApp* self)
{
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    return
        priv->user_info &&
        !utils_str_empty(priv->user_info->oauth_token) &&
        !utils_str_empty(priv->user_info->name);
}

GtUserInfo* gt_user_info_new()
{
    return g_slice_new0(GtUserInfo);
}
void
gt_user_info_free(GtUserInfo* info)
{
    g_free(info->name);
    g_free(info->oauth_token);
    g_free(info->display_name);
    g_free(info->bio);
    g_free(info->logo_url);
    g_free(info->type);
    g_free(info->email);

    g_date_time_unref(info->created_at);
    g_date_time_unref(info->updated_at);

    g_slice_free(GtUserInfo, info);
}
