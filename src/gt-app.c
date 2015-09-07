#include "gt-app.h"
#include "gt-win.h"

typedef struct
{
    GtWin* win;
} GtAppPrivate;

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

GtApp*
gt_app_new(void)
{
    return g_object_new(GT_TYPE_APP, 
                        "application-id", "com.gnome-twitch.app",
                        NULL);
}


static void
quit_cb(GSimpleAction* action,
        GVariant* par,
        gpointer udata)
{
    GtApp* self = GT_APP(udata);

    g_application_quit(G_APPLICATION(self));
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
    GtkBuilder* menu_bld;
    GMenuModel* app_menu;

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
}

static void
startup(GApplication* app)
{
    GtApp* self = GT_APP(app);
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    G_APPLICATION_CLASS(gt_app_parent_class)->startup(app);

}

static void
finalize(GObject* object)
{
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

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;
}

static void
gt_app_init(GtApp* self)
{
    GtAppPrivate* priv = gt_app_get_instance_private(self);

    self->twitch = gt_twitch_new();

    /* g_signal_connect(self, "activate", G_CALLBACK(activate), NULL); */
    /* g_signal_connect(self, "startup", G_CALLBACK(startup), NULL); */
}
