#include "gt-channels-container-game.h"
#include "gt-twitch.h"
#include "gt-app.h"
#include <glib/gi18n.h>

#define TAG "GtChannelsContainerGame"
#include "utils.h"

#define PCLASS GT_CHANNELS_CONTAINER_CLASS(gt_channels_container_game_parent_class)

typedef struct
{
    gchar* game;
    gint page;

    GCancellable* cancel;
} GtChannelsContainerGamePrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtChannelsContainerGame, gt_channels_container_game, GT_TYPE_CHANNELS_CONTAINER)

enum
{
    PROP_0,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtChannelsContainerGame*
gt_channels_container_game_new(void)
{
    return g_object_new(GT_TYPE_CHANNELS_CONTAINER_GAME,
                        NULL);
}

static void
top_channels_cb(GObject* source,
                GAsyncResult* res,
                gpointer udata)
{
    GtChannelsContainerGame* self = GT_CHANNELS_CONTAINER_GAME(udata);
    GtChannelsContainerGamePrivate* priv = gt_channels_container_game_get_instance_private(self);
    GError* err = NULL;

    GList* new = g_task_propagate_pointer(G_TASK(res), &err);

    if (err)
        g_error_free(err);
    else
        PCLASS->append_channels(GT_CHANNELS_CONTAINER(self), new);

    PCLASS->show_load_spinner(GT_CHANNELS_CONTAINER(self), FALSE);
}

static void
get_channels(GtChannelsContainerGame* self)
{
    GtChannelsContainerGamePrivate* priv = gt_channels_container_game_get_instance_private(self);

    PCLASS->show_load_spinner(GT_CHANNELS_CONTAINER(self), TRUE);

    g_cancellable_cancel(priv->cancel);
    g_object_unref(priv->cancel);
    priv->cancel = g_cancellable_new();

    gt_twitch_top_channels_async(main_app->twitch,
                                 MAX_QUERY,
                                 (priv->page++)*MAX_QUERY,
                                 priv->game,
                                 priv->cancel,
                                 (GAsyncReadyCallback) top_channels_cb,
                                 self);
}

static void
bottom_edge_reached(GtChannelsContainer* container)
{
    GtChannelsContainerGame* self = GT_CHANNELS_CONTAINER_GAME(container);
    GtChannelsContainerGamePrivate* priv = gt_channels_container_game_get_instance_private(self);

    get_channels(self);
}

static void
refresh(GtChannelsContainer* container)
{
    GtChannelsContainerGame* self = GT_CHANNELS_CONTAINER_GAME(container);
    GtChannelsContainerGamePrivate* priv = gt_channels_container_game_get_instance_private(self);

    priv->page = 0;

    utils_container_clear(GTK_CONTAINER(PCLASS->get_channels_flow(container)));

    get_channels(self);
}

static void
filter(GtChannelsContainer* container, const gchar* query)
{
    GtChannelsContainerGame* self = GT_CHANNELS_CONTAINER_GAME(container);
    GtChannelsContainerGamePrivate* priv = gt_channels_container_game_get_instance_private(self);

    g_free(priv->game);
    priv->page = 0;
    priv->game = g_strdup(query);

    utils_container_clear(GTK_CONTAINER(PCLASS->get_channels_flow(container)));

    get_channels(self);
}

static void
finalize(GObject* object)
{
    GtChannelsContainerGame* self = (GtChannelsContainerGame*) object;
    GtChannelsContainerGamePrivate* priv = gt_channels_container_game_get_instance_private(self);

    G_OBJECT_CLASS(gt_channels_container_game_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtChannelsContainerGame* self = GT_CHANNELS_CONTAINER_GAME(obj);
    GtChannelsContainerGamePrivate* priv = gt_channels_container_game_get_instance_private(self);

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
    GtChannelsContainerGame* self = GT_CHANNELS_CONTAINER_GAME(obj);
    GtChannelsContainerGamePrivate* priv = gt_channels_container_game_get_instance_private(self);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_channels_container_game_class_init(GtChannelsContainerGameClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    GT_CHANNELS_CONTAINER_CLASS(klass)->bottom_edge_reached = bottom_edge_reached;
    GT_CHANNELS_CONTAINER_CLASS(klass)->refresh = refresh;
    GT_CHANNELS_CONTAINER_CLASS(klass)->filter = filter;
}

static void
gt_channels_container_game_init(GtChannelsContainerGame* self)
{
    GtChannelsContainerGamePrivate* priv = gt_channels_container_game_get_instance_private(self);

    priv->game = NULL;
    priv->page = 0;
    priv->cancel = g_cancellable_new();

    PCLASS->set_loading_info(GT_CHANNELS_CONTAINER(self), _("Loading channels"));
}
