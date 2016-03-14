#include "gt-games-container-search.h"
#include "gt-app.h"
#include "gt-twitch.h"
#include "utils.h"
#include <string.h>

#define PCLASS GT_GAMES_CONTAINER_CLASS(gt_games_container_search_parent_class)

#define FILTER_TIMEOUT_MS 500

typedef struct
{
    gchar* query;
    gint page;

    gint filter_timeout_id;

    GCancellable* cancel;
} GtGamesContainerSearchPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtGamesContainerSearch, gt_games_container_search, GT_TYPE_GAMES_CONTAINER)

enum
{
    PROP_0,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtGamesContainerSearch*
gt_games_container_search_new(void)
{
    return g_object_new(GT_TYPE_GAMES_CONTAINER_SEARCH,
                        NULL);
}

static void
search_games_cb(GObject* source,
                   GAsyncResult* res,
                   gpointer udata)
{
    GtGamesContainerSearch* self = GT_GAMES_CONTAINER_SEARCH(udata);
    GtGamesContainerSearchPrivate* priv = gt_games_container_search_get_instance_private(self);
    GError* err = NULL;

    GList* new = g_task_propagate_pointer(G_TASK(res), &err);

    if (err)
        g_error_free(err);
    else
        PCLASS->append_games(GT_GAMES_CONTAINER(self), new);

    if (g_cancellable_is_cancelled(priv->cancel))
        PCLASS->show_load_spinner(GT_GAMES_CONTAINER(self), FALSE);
}

static void
get_games(GtGamesContainerSearch* self, const gchar* query)
{
    GtGamesContainerSearchPrivate* priv = gt_games_container_search_get_instance_private(self);

    if (!query || strlen(query) == 0)
        return;

    g_cancellable_cancel(priv->cancel);
    g_object_unref(priv->cancel);
    priv->cancel = g_cancellable_new();

    gt_twitch_search_games_async(main_app->twitch,
                                 query,
                                 MAX_QUERY,
                                 (priv->page++)*MAX_QUERY,
                                 priv->cancel,
                                 (GAsyncReadyCallback) search_games_cb,
                                 self);
}

static gboolean
filter_timeout_cb(gpointer udata)
{
    GtGamesContainerSearch* self = GT_GAMES_CONTAINER_SEARCH(udata);
    GtGamesContainerSearchPrivate* priv = gt_games_container_search_get_instance_private(self);

    get_games(self, priv->query);
    priv->filter_timeout_id = -1;

    return G_SOURCE_REMOVE;
}

static void
filter(GtGamesContainer* container, const gchar* query)
{
    GtGamesContainerSearch* self = GT_GAMES_CONTAINER_SEARCH(container);
    GtGamesContainerSearchPrivate* priv = gt_games_container_search_get_instance_private(self);

    if (priv->filter_timeout_id > 0)
        g_source_remove(priv->filter_timeout_id);

    PCLASS->show_load_spinner(GT_GAMES_CONTAINER(self), query && strlen(query) > 0);

    priv->page = 0;
    g_free(priv->query);
    priv->query = g_strdup(query);

    utils_container_clear(GTK_CONTAINER(PCLASS->get_games_flow(container)));

    priv->filter_timeout_id = g_timeout_add(FILTER_TIMEOUT_MS, filter_timeout_cb, self);
}

static void
refresh(GtGamesContainer* container)
{
    GtGamesContainerSearch* self = GT_GAMES_CONTAINER_SEARCH(container);
    GtGamesContainerSearchPrivate* priv = gt_games_container_search_get_instance_private(self);

    gchar* tmpstr = g_strdup(priv->query);
    filter(container, tmpstr);
    g_free(tmpstr);
}

static void
bottom_edge_reached(GtGamesContainer* container)
{
    GtGamesContainerSearch* self = GT_GAMES_CONTAINER_SEARCH(container);
    GtGamesContainerSearchPrivate* priv = gt_games_container_search_get_instance_private(self);

    get_games(self, priv->query);

    PCLASS->show_load_spinner(GT_GAMES_CONTAINER(self), TRUE);
}

static void
finalize(GObject* object)
{
    GtGamesContainerSearch* self = (GtGamesContainerSearch*) object;
    GtGamesContainerSearchPrivate* priv = gt_games_container_search_get_instance_private(self);

    G_OBJECT_CLASS(gt_games_container_search_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtGamesContainerSearch* self = GT_GAMES_CONTAINER_SEARCH(obj);
    GtGamesContainerSearchPrivate* priv = gt_games_container_search_get_instance_private(self);

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
    GtGamesContainerSearch* self = GT_GAMES_CONTAINER_SEARCH(obj);
    GtGamesContainerSearchPrivate* priv = gt_games_container_search_get_instance_private(self);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_games_container_search_class_init(GtGamesContainerSearchClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    GT_GAMES_CONTAINER_CLASS(klass)->bottom_edge_reached = bottom_edge_reached;
    GT_GAMES_CONTAINER_CLASS(klass)->refresh = refresh;
    GT_GAMES_CONTAINER_CLASS(klass)->filter = filter;
}

static void
gt_games_container_search_init(GtGamesContainerSearch* self)
{
    GtGamesContainerSearchPrivate* priv = gt_games_container_search_get_instance_private(self);

    priv->page = 0;
    priv->filter_timeout_id = -1;
    priv->cancel = g_cancellable_new();
}
