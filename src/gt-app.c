#include "gt-app.h"
#include "gt-win.h"
#include <glib/gstdio.h>
#include <errno.h>
#include <string.h>

#define DATA_DIR g_build_filename(g_get_user_data_dir(), "gnome-twitch", NULL)

typedef struct
{
    GtWin* win;

    gchar* oauth_token;
    gchar* user_name;
} GtAppPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtApp, gt_app, GTK_TYPE_APPLICATION)

enum
{
    PROP_0,
    PROP_OAUTH_TOKEN,
    PROP_USER_NAME,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

enum
{
    NUM_SIGS
};

static guint sigs[NUM_SIGS];

GtApp*
gt_app_new(void)
{
    return g_object_new(GT_TYPE_APP,
                        "application-id", "com.gnome-twitch.app",
                        NULL);
}

static void
init_dirs()
{
    gchar* fp = DATA_DIR;

    int err = g_mkdir(fp, 0777);

    if (err != 0 && g_file_error_from_errno(errno) != G_FILE_ERROR_EXIST)
    {
        g_warning("{GtApp} Error creating data dir");
    }

    g_free(fp);
}


static void
quit_cb(GSimpleAction* action,
        GVariant* par,
        gpointer udata)
{
    g_message("{%s} Quitting", "GtApp");

    GtApp* self = GT_APP(udata);

    g_application_quit(G_APPLICATION(self));
}

static void
oauth_token_set_cb(GObject* source,
                   GParamSpec* pspec,
                   gpointer udata)
{
    GtApp* self = GT_APP(udata);
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    if (priv->oauth_token && strlen(priv->oauth_token) > 0 &&
        priv->user_name && strlen(priv->user_name) > 0)
        gt_twitch_chat_client_connect(self->chat);
}

static GActionEntry app_actions[] =
{
    {"quit", quit_cb, NULL, NULL, NULL}
};

static void
activate(GApplication* app)
{
    g_message("{%s} Activate", "GtApp");

    GtApp* self = GT_APP(app);
    GtAppPrivate* priv = gt_app_get_instance_private(self);
    GtkBuilder* menu_bld;
    GMenuModel* app_menu;

    init_dirs();

    g_action_map_add_action_entries(G_ACTION_MAP(self),
                                    app_actions,
                                    G_N_ELEMENTS(app_actions),
                                    self);

    menu_bld = gtk_builder_new_from_resource("/com/gnome-twitch/ui/app-menu.ui");
    app_menu = G_MENU_MODEL(gtk_builder_get_object(menu_bld, "app_menu"));
    gtk_application_set_app_menu(GTK_APPLICATION(app), G_MENU_MODEL(app_menu));
    g_object_unref(menu_bld);

    priv->win = gt_win_new(self);

    gtk_window_present(GTK_WINDOW(priv->win));

    g_settings_bind(self->settings, "user-name",
                    self, "user-name",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(self->settings, "oauth-token",
                    self, "oauth-token",
                    G_SETTINGS_BIND_DEFAULT);
}

static void
startup(GApplication* app)
{
    GtApp* self = GT_APP(app);
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    self->fav_mgr = gt_favourites_manager_new();
    gt_favourites_manager_load(self->fav_mgr);

    G_APPLICATION_CLASS(gt_app_parent_class)->startup(app);
}

static void
finalize(GObject* object)
{
    g_message("Startup");

    GtApp* self = (GtApp*) object;
    GtAppPrivate* priv = gt_app_get_instance_private(self);

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
        case PROP_OAUTH_TOKEN:
            g_value_set_string(val, priv->oauth_token);
            break;
        case PROP_USER_NAME:
            g_value_set_string(val, priv->user_name);
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
    GtApp* self = GT_APP(obj);
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    switch (prop)
    {
        case PROP_OAUTH_TOKEN:
            g_free(priv->oauth_token);
            priv->oauth_token = g_value_dup_string(val);
            break;
        case PROP_USER_NAME:
            g_free(priv->user_name);
            priv->user_name = g_value_dup_string(val);
            break;
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

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_OAUTH_TOKEN] = g_param_spec_string("oauth-token",
                                                  "Oauth token",
                                                  "Twitch Oauth token",
                                                  NULL,
                                                  G_PARAM_READWRITE);
    props[PROP_USER_NAME] = g_param_spec_string("user-name",
                                                "User name",
                                                "User name",
                                                NULL,
                                                G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, NUM_PROPS, props);
}

static void
gt_app_init(GtApp* self)
{
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    self->twitch = gt_twitch_new();
    self->settings = g_settings_new("com.gnome-twitch.app");
    self->chat = gt_twitch_chat_client_new();

    g_signal_connect(self, "notify::oauth-token", G_CALLBACK(oauth_token_set_cb), self);

    /* g_signal_connect(self, "activate", G_CALLBACK(activate), NULL); */
    /* g_signal_connect(self, "startup", G_CALLBACK(startup), NULL); */
}

const gchar*
gt_app_get_user_name(GtApp* self)
{
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    return priv->user_name;
}
