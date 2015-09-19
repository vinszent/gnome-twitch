#include "gt-games-view-child.h"
#include "utils.h"

typedef struct
{
    GtGame* game;

    GtkWidget* preview_image;
    GtkWidget* middle_revealer;
    GtkWidget* name_label;
    GtkWidget* event_box;
} GtGamesViewChildPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtGamesViewChild, gt_games_view_child, GTK_TYPE_FLOW_BOX_CHILD)

enum 
{
    PROP_0,
    PROP_GAME,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtGamesViewChild*
gt_games_view_child_new(GtGame* game)
{
    return g_object_new(GT_TYPE_GAMES_VIEW_CHILD, 
                        "game", game,
                        NULL);
}

static void
motion_enter_cb(GtkWidget* widget,
                GdkEvent* evt,
                gpointer udata)
{
    GtGamesViewChild* self = GT_GAMES_VIEW_CHILD(udata);
    GtGamesViewChildPrivate* priv = gt_games_view_child_get_instance_private(self);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->middle_revealer), TRUE);
}

static void
motion_leave_cb(GtkWidget* widget,
                GdkEvent* evt,
                gpointer udata)
{
    GtGamesViewChild* self = GT_GAMES_VIEW_CHILD(udata);
    GtGamesViewChildPrivate* priv = gt_games_view_child_get_instance_private(self);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->middle_revealer), FALSE);
}

static void
finalize(GObject* object)
{
    GtGamesViewChild* self = (GtGamesViewChild*) object;
    GtGamesViewChildPrivate* priv = gt_games_view_child_get_instance_private(self);

    g_object_unref(priv->game);

    G_OBJECT_CLASS(gt_games_view_child_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtGamesViewChild* self = GT_GAMES_VIEW_CHILD(obj);
    GtGamesViewChildPrivate* priv = gt_games_view_child_get_instance_private(self);

    switch (prop)
    {
        case PROP_GAME:
            g_value_set_object(val, priv->game);
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
    GtGamesViewChild* self = GT_GAMES_VIEW_CHILD(obj);
    GtGamesViewChildPrivate* priv = gt_games_view_child_get_instance_private(self);

    switch (prop)
    {
        case PROP_GAME:
            if (priv->game)
                g_object_unref(priv->game);
            priv->game = g_value_ref_sink_object(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
constructed(GObject* obj)
{
    GtGamesViewChild* self = GT_GAMES_VIEW_CHILD(obj);
    GtGamesViewChildPrivate* priv = gt_games_view_child_get_instance_private(self);

    g_object_bind_property(priv->game, "name",
                           priv->name_label, "label",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(priv->game, "preview",
                           priv->preview_image, "pixbuf",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

    G_OBJECT_CLASS(gt_games_view_child_parent_class)->constructed(obj);
}

static void
gt_games_view_child_class_init(GtGamesViewChildClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;
    object_class->constructed = constructed;

    props[PROP_GAME] = g_param_spec_object("game",
                                           "Game",
                                           "Associated game",
                                           GT_TYPE_GAME,
                                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties(object_class,
                                      NUM_PROPS,
                                      props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), 
                                                "/com/gnome-twitch/ui/gt-games-view-child.ui");
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), motion_enter_cb);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), motion_leave_cb);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtGamesViewChild, preview_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtGamesViewChild, middle_revealer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtGamesViewChild, name_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtGamesViewChild, event_box);
}

static void
gt_games_view_child_init(GtGamesViewChild* self)
{
    GtGamesViewChildPrivate* priv = gt_games_view_child_get_instance_private(self);

    gtk_widget_init_template(GTK_WIDGET(self));
}

void
gt_games_view_child_hide_overlay(GtGamesViewChild* self)
{
    GtGamesViewChildPrivate* priv = gt_games_view_child_get_instance_private(self);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->middle_revealer), FALSE);
}
