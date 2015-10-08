#include "gt-channels-container-top.h"
#include "gt-app.h"
#include "gt-twitch.h"
#include "utils.h"

#define PCLASS GT_CHANNELS_CONTAINER_CLASS(gt_channels_container_top_parent_class)

typedef struct
{
    gint page;

    GCancellable* cancel;
} GtChannelsContainerTopPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtChannelsContainerTop, gt_channels_container_top, GT_TYPE_CHANNELS_CONTAINER)

enum 
{
    PROP_0,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtChannelsContainerTop*
gt_channels_container_top_new(void)
{
    return g_object_new(GT_TYPE_CHANNELS_CONTAINER_TOP, 
                        NULL);
}

static void
top_channels_cb(GObject* source,
                GAsyncResult* res,
                gpointer udata)
{
    GtChannelsContainerTop* self = GT_CHANNELS_CONTAINER_TOP(udata);
    GtChannelsContainerTopPrivate* priv = gt_channels_container_top_get_instance_private(self);
    GError* err = NULL;

    GList* new = g_task_propagate_pointer(G_TASK(res), &err);

    if (err)
        g_error_free(err);
    else
        PCLASS->append_channels(GT_CHANNELS_CONTAINER(self), new);

    PCLASS->show_load_spinner(GT_CHANNELS_CONTAINER(self), FALSE);
}

static void
get_channels(GtChannelsContainerTop* self)
{
    GtChannelsContainerTopPrivate* priv = gt_channels_container_top_get_instance_private(self);

    PCLASS->show_load_spinner(GT_CHANNELS_CONTAINER(self), TRUE);

    g_cancellable_cancel(priv->cancel);
    g_object_unref(priv->cancel);
    priv->cancel = g_cancellable_new();

    gt_twitch_top_channels_async(main_app->twitch, 
                                 MAX_QUERY, 
                                 (priv->page++)*MAX_QUERY, 
                                 NO_GAME, priv->cancel, 
                                 (GAsyncReadyCallback) top_channels_cb, 
                                 self);
}

static void
bottom_edge_reached(GtChannelsContainer* container)
{
    GtChannelsContainerTop* self = GT_CHANNELS_CONTAINER_TOP(container);
    GtChannelsContainerTopPrivate* priv = gt_channels_container_top_get_instance_private(self);

    get_channels(self);
}

static void
refresh(GtChannelsContainer* container)
{
    GtChannelsContainerTop* self = GT_CHANNELS_CONTAINER_TOP(container);
    GtChannelsContainerTopPrivate* priv = gt_channels_container_top_get_instance_private(self);

    priv->page = 0;

    utils_container_clear(GTK_CONTAINER(PCLASS->get_channels_flow(container)));

    get_channels(self);
}

static void
finalize(GObject* object)
{
    GtChannelsContainerTop* self = (GtChannelsContainerTop*) object;
    GtChannelsContainerTopPrivate* priv = gt_channels_container_top_get_instance_private(self);

    G_OBJECT_CLASS(gt_channels_container_top_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtChannelsContainerTop* self = GT_CHANNELS_CONTAINER_TOP(obj);
    GtChannelsContainerTopPrivate* priv = gt_channels_container_top_get_instance_private(self);

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
    GtChannelsContainerTop* self = GT_CHANNELS_CONTAINER_TOP(obj);
    GtChannelsContainerTopPrivate* priv = gt_channels_container_top_get_instance_private(self);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_channels_container_top_class_init(GtChannelsContainerTopClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    GT_CHANNELS_CONTAINER_CLASS(klass)->bottom_edge_reached = bottom_edge_reached;
    GT_CHANNELS_CONTAINER_CLASS(klass)->refresh = refresh;
}

static void
gt_channels_container_top_init(GtChannelsContainerTop* self)
{
    GtChannelsContainerTopPrivate* priv = gt_channels_container_top_get_instance_private(self);

    priv->page = 0;
    priv->cancel = g_cancellable_new();

    get_channels(self);
}
