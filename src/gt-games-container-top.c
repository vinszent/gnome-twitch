#include "gt-games-container-top.h"
#include "gt-app.h"
#include "gt-twitch.h"
#include "utils.h"

#define PCLASS GT_GAMES_CONTAINER_CLASS(gt_games_container_top_parent_class)

typedef struct
{
    gint page;

    GCancellable* cancel;
} GtGamesContainerTopPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtGamesContainerTop, gt_games_container_top, GT_TYPE_GAMES_CONTAINER)

enum 
{
    PROP_0,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtGamesContainerTop*
gt_games_container_top_new(void)
{
    return g_object_new(GT_TYPE_GAMES_CONTAINER_TOP, 
                        NULL);
}

static void
top_games_cb(GObject* source,
             GAsyncResult* res,
             gpointer udata)
{
    GtGamesContainerTop* self = GT_GAMES_CONTAINER_TOP(udata);
    GtGamesContainerTopPrivate* priv = gt_games_container_top_get_instance_private(self);
    GError* err = NULL;

    GList* new = g_task_propagate_pointer(G_TASK(res), &err);

    if (err)
        g_error_free(err);
    else
        PCLASS->append_games(GT_GAMES_CONTAINER(self), new);

    PCLASS->show_load_spinner(GT_GAMES_CONTAINER(self), FALSE);
}

static void
get_games(GtGamesContainerTop* self)
{
    GtGamesContainerTopPrivate* priv = gt_games_container_top_get_instance_private(self);

    PCLASS->show_load_spinner(GT_GAMES_CONTAINER(self), TRUE);

    g_cancellable_cancel(priv->cancel);
    g_object_unref(priv->cancel);
    priv->cancel = g_cancellable_new();

    gt_twitch_top_games_async(main_app->twitch, 
                              MAX_QUERY, 
                              (priv->page++)*MAX_QUERY, 
                              priv->cancel, 
                              (GAsyncReadyCallback) top_games_cb, 
                              self);
}

static void
bottom_edge_reached(GtGamesContainer* container)
{
    GtGamesContainerTop* self = GT_GAMES_CONTAINER_TOP(container);
    GtGamesContainerTopPrivate* priv = gt_games_container_top_get_instance_private(self);

    get_games(self);
}

static void
refresh(GtGamesContainer* container)
{
    GtGamesContainerTop* self = GT_GAMES_CONTAINER_TOP(container);
    GtGamesContainerTopPrivate* priv = gt_games_container_top_get_instance_private(self);

    priv->page = 0;

    utils_container_clear(GTK_CONTAINER(PCLASS->get_games_flow(container)));

    get_games(self);
}

static void
finalize(GObject* object)
{
    GtGamesContainerTop* self = (GtGamesContainerTop*) object;
    GtGamesContainerTopPrivate* priv = gt_games_container_top_get_instance_private(self);

    G_OBJECT_CLASS(gt_games_container_top_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtGamesContainerTop* self = GT_GAMES_CONTAINER_TOP(obj);
    GtGamesContainerTopPrivate* priv = gt_games_container_top_get_instance_private(self);

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
    GtGamesContainerTop* self = GT_GAMES_CONTAINER_TOP(obj);
    GtGamesContainerTopPrivate* priv = gt_games_container_top_get_instance_private(self);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_games_container_top_class_init(GtGamesContainerTopClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    GT_GAMES_CONTAINER_CLASS(klass)->bottom_edge_reached = bottom_edge_reached;
    GT_GAMES_CONTAINER_CLASS(klass)->refresh = refresh;
}

static void
gt_games_container_top_init(GtGamesContainerTop* self)
{
    GtGamesContainerTopPrivate* priv = gt_games_container_top_get_instance_private(self);

    priv->page = 0;
    priv->cancel = g_cancellable_new();

    get_games(self);
}
