#include "gt-games-container-child.h"

#define TAG "GtGamesContainerChild"
#include "utils.h"

typedef struct
{
    GtGame* game;

    GtkWidget* cover_stack;
    GtkWidget* cover_image;
    GtkWidget* cover_overlay_revealer;
    GtkWidget* name_label;
} GtGamesContainerChildPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtGamesContainerChild, gt_games_container_child, GTK_TYPE_FLOW_BOX_CHILD)

enum
{
    PROP_0,
    PROP_GAME,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtGamesContainerChild*
gt_games_container_child_new(GtGame* game)
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
    GtGamesContainerChild* self = GT_GAMES_CONTAINER_CHILD(udata);
    GtGamesContainerChildPrivate* priv = gt_games_container_child_get_instance_private(self);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->cover_overlay_revealer), TRUE);
}

static void
motion_leave_cb(GtkWidget* widget,
                GdkEvent* evt,
                gpointer udata)
{
    GtGamesContainerChild* self = GT_GAMES_CONTAINER_CHILD(udata);
    GtGamesContainerChildPrivate* priv = gt_games_container_child_get_instance_private(self);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->cover_overlay_revealer), FALSE);
}

static void
updating_cb(GObject* source,
    GParamSpec* pspec, gpointer udata)
{
    g_assert(GT_IS_GAMES_CONTAINER_CHILD(udata));
    g_assert(GT_IS_GAME(source));

    GtGamesContainerChild* self = GT_GAMES_CONTAINER_CHILD(udata);
    GtGamesContainerChildPrivate* priv = gt_games_container_child_get_instance_private(self);
    GtGame* game = GT_GAME(source);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->cover_stack),
        gt_game_get_updating(game) ? "load-spinner" : "cover");
}

static void
finalize(GObject* object)
{
    GtGamesContainerChild* self = (GtGamesContainerChild*) object;
    GtGamesContainerChildPrivate* priv = gt_games_container_child_get_instance_private(self);

    g_object_unref(priv->game);

    G_OBJECT_CLASS(gt_games_container_child_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtGamesContainerChild* self = GT_GAMES_CONTAINER_CHILD(obj);
    GtGamesContainerChildPrivate* priv = gt_games_container_child_get_instance_private(self);

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
    GtGamesContainerChild* self = GT_GAMES_CONTAINER_CHILD(obj);
    GtGamesContainerChildPrivate* priv = gt_games_container_child_get_instance_private(self);

    switch (prop)
    {
        case PROP_GAME:
            g_clear_object(&priv->game);
            priv->game = utils_value_ref_sink_object(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
constructed(GObject* obj)
{
    GtGamesContainerChild* self = GT_GAMES_CONTAINER_CHILD(obj);
    GtGamesContainerChildPrivate* priv = gt_games_container_child_get_instance_private(self);

    g_object_bind_property(priv->game, "name",
                           priv->name_label, "label",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(priv->game, "preview",
                           priv->cover_image, "pixbuf",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

    g_signal_connect(priv->game, "notify::updating", G_CALLBACK(updating_cb), self);

    updating_cb(G_OBJECT(priv->game), NULL, self);

    G_OBJECT_CLASS(gt_games_container_child_parent_class)->constructed(obj);
}

static void
gt_games_container_child_class_init(GtGamesContainerChildClass* klass)
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
                                                "/com/vinszent/GnomeTwitch/ui/gt-games-container-child.ui");
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), motion_enter_cb);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), motion_leave_cb);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtGamesContainerChild, cover_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtGamesContainerChild, cover_overlay_revealer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtGamesContainerChild, name_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtGamesContainerChild, cover_stack);
}

static void
gt_games_container_child_init(GtGamesContainerChild* self)
{
    GtGamesContainerChildPrivate* priv = gt_games_container_child_get_instance_private(self);

    gtk_widget_init_template(GTK_WIDGET(self));
}

void
gt_games_container_child_hide_overlay(GtGamesContainerChild* self)
{
    GtGamesContainerChildPrivate* priv = gt_games_container_child_get_instance_private(self);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->cover_overlay_revealer), FALSE);
}


GtGame*
gt_games_container_child_get_game(GtGamesContainerChild* self)
{
    g_assert(GT_IS_GAMES_CONTAINER_CHILD(self));

    GtGamesContainerChildPrivate* priv = gt_games_container_child_get_instance_private(self);

    return priv->game;
}
