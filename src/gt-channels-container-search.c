#include "gt-channels-container-search.h"
#include "gt-twitch.h"
#include "gt-app.h"
#include <string.h>
#include <glib/gi18n.h>
#include "utils.h"

#define TAG "GtChannelsContainerSearch"
#include "gnome-twitch/gt-log.h"

#define PCLASS GT_CHANNELS_CONTAINER_CLASS(gt_channels_container_search_parent_class)

typedef struct
{
    gchar* query;
    gint page;

    GCancellable* cancel;
} GtChannelsContainerSearchPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtChannelsContainerSearch, gt_channels_container_search, GT_TYPE_CHANNELS_CONTAINER)

enum
{
    PROP_0,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtChannelsContainerSearch*
gt_channels_container_search_new(void)
{
    return g_object_new(GT_TYPE_CHANNELS_CONTAINER_SEARCH,
                        NULL);
}

static void
search_channels_cb(GObject* source,
                   GAsyncResult* res,
                   gpointer udata)
{
    GtChannelsContainerSearch* self = GT_CHANNELS_CONTAINER_SEARCH(udata);
    GtChannelsContainerSearchPrivate* priv = gt_channels_container_search_get_instance_private(self);
    GError* err = NULL;

    GList* new = g_task_propagate_pointer(G_TASK(res), &err);

    PCLASS->append_channels(GT_CHANNELS_CONTAINER(self), new);

    if (!err || err->code != 19)
    {
            PCLASS->show_load_spinner(GT_CHANNELS_CONTAINER(self), FALSE);
            PCLASS->check_empty(GT_CHANNELS_CONTAINER(self));
    }

    if (err)
        g_error_free(err);
}

static void
get_channels(GtChannelsContainerSearch* self, const gchar* query)
{
    GtChannelsContainerSearchPrivate* priv = gt_channels_container_search_get_instance_private(self);

    g_cancellable_cancel(priv->cancel);
    g_object_unref(priv->cancel);
    priv->cancel = g_cancellable_new();

    if (!query || strlen(query) == 0)
    {
        PCLASS->check_empty(GT_CHANNELS_CONTAINER(self));
        return;
    }

    PCLASS->show_load_spinner(GT_CHANNELS_CONTAINER(self), TRUE);

    gt_twitch_search_channels_async(main_app->twitch,
                                    query,
                                    MAX_QUERY,
                                    (priv->page++)*MAX_QUERY,
                                    priv->cancel,
                                    (GAsyncReadyCallback) search_channels_cb,
                                    self);
}

static void
filter(GtChannelsContainer* container, const gchar* query)
{
    GtChannelsContainerSearch* self = GT_CHANNELS_CONTAINER_SEARCH(container);
    GtChannelsContainerSearchPrivate* priv = gt_channels_container_search_get_instance_private(self);

    priv->page = 0;
    g_free(priv->query);
    priv->query = g_strdup(query);

    utils_container_clear(GTK_CONTAINER(PCLASS->get_channels_flow(container)));

    get_channels(self, query);
}

static void
refresh(GtChannelsContainer* container)
{
    GtChannelsContainerSearch* self = GT_CHANNELS_CONTAINER_SEARCH(container);
    GtChannelsContainerSearchPrivate* priv = gt_channels_container_search_get_instance_private(self);

    gchar* tmpstr = g_strdup(priv->query);
    filter(container, tmpstr);
    g_free(tmpstr);
}

static void
bottom_edge_reached(GtChannelsContainer* container)
{
    GtChannelsContainerSearch* self = GT_CHANNELS_CONTAINER_SEARCH(container);
    GtChannelsContainerSearchPrivate* priv = gt_channels_container_search_get_instance_private(self);

    get_channels(self, priv->query);

    PCLASS->show_load_spinner(GT_CHANNELS_CONTAINER(self), TRUE);
}

static void
finalize(GObject* object)
{
    GtChannelsContainerSearch* self = (GtChannelsContainerSearch*) object;
    GtChannelsContainerSearchPrivate* priv = gt_channels_container_search_get_instance_private(self);

    G_OBJECT_CLASS(gt_channels_container_search_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtChannelsContainerSearch* self = GT_CHANNELS_CONTAINER_SEARCH(obj);
    GtChannelsContainerSearchPrivate* priv = gt_channels_container_search_get_instance_private(self);

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
    GtChannelsContainerSearch* self = GT_CHANNELS_CONTAINER_SEARCH(obj);
    GtChannelsContainerSearchPrivate* priv = gt_channels_container_search_get_instance_private(self);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_channels_container_search_class_init(GtChannelsContainerSearchClass* klass)
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
gt_channels_container_search_init(GtChannelsContainerSearch* self)
{
    GtChannelsContainerSearchPrivate* priv = gt_channels_container_search_get_instance_private(self);

    PCLASS->set_empty_info(GT_CHANNELS_CONTAINER(self),
                           "edit-find-symbolic",
                           _("No channels found"),
                           _("Try a different search"));
    PCLASS->set_loading_info(GT_CHANNELS_CONTAINER(self), _("Searching channels"));

    PCLASS->check_empty(GT_CHANNELS_CONTAINER(self));

    priv->page = 0;
    priv->cancel = g_cancellable_new();
}
