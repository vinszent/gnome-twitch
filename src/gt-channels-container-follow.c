#include "gt-channels-container-follow.h"
#include "gt-app.h"
#include <glib/gi18n.h>

#define PCLASS GT_CHANNELS_CONTAINER_CLASS(gt_channels_container_follow_parent_class)

typedef struct
{
    gchar* search_query;
} GtChannelsContainerFollowPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtChannelsContainerFollow, gt_channels_container_follow, GT_TYPE_CHANNELS_CONTAINER)

enum
{
    PROP_0,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtChannelsContainerFollow*
gt_channels_container_follow_new(void)
{
    return g_object_new(GT_TYPE_CHANNELS_CONTAINER_FOLLOW,
                        NULL);
}

static void
bottom_edge_reached(GtChannelsContainer* container)
{
    // Do nothing
}

static void
refresh(GtChannelsContainer* container)
{
    //TODO: Force follows refresh
}

static void
filter(GtChannelsContainer* container, const gchar* query)
{
    GtChannelsContainerFollow* self = GT_CHANNELS_CONTAINER_FOLLOW(container);
    GtChannelsContainerFollowPrivate* priv = gt_channels_container_follow_get_instance_private(self);

    g_free(priv->search_query);
    priv->search_query = g_strdup(query);

    gtk_flow_box_invalidate_filter(PCLASS->get_channels_flow(container));
}

static gboolean
filter_follows_by_name(GtkFlowBoxChild* _child,
                          gpointer udata)
{
    GtChannelsContainerChild* child = GT_CHANNELS_CONTAINER_CHILD(_child);
    GtChannelsContainerFollow* self = GT_CHANNELS_CONTAINER_FOLLOW(udata);
    GtChannelsContainerFollowPrivate* priv = gt_channels_container_follow_get_instance_private(self);
    GtChannel* chan;
    gchar* name;
    gboolean ret = FALSE;

    g_object_get(child, "channel", &chan, NULL);
    g_object_get(chan, "name", &name, NULL);

    if (!priv->search_query)
        ret = TRUE;
    else
        ret = g_strrstr(name, priv->search_query) != NULL;

    g_free(name);
    g_object_unref(chan);

    return ret;
}

static gint
sort_follows_by_name_and_online(GtkFlowBoxChild* _child1,
                                   GtkFlowBoxChild* _child2,
                                   gpointer udata)
{
    GtChannelsContainerFollow* self = GT_CHANNELS_CONTAINER_FOLLOW(udata);
    GtChannelsContainerFollowPrivate* priv = gt_channels_container_follow_get_instance_private(self);
    GtChannelsContainerChild* child1 = GT_CHANNELS_CONTAINER_CHILD(_child1);
    GtChannelsContainerChild* child2 = GT_CHANNELS_CONTAINER_CHILD(_child2);
    GtChannel* chan1;
    GtChannel* chan2;
    gboolean online1;
    gboolean online2;
    gchar* name1;
    gchar* name2;
    gint ret = 0;

    g_object_get(child1, "channel", &chan1, NULL);
    g_object_get(child2, "channel", &chan2, NULL);
    g_object_get(chan1,
                 "online", &online1,
                 "name", &name1,
                 NULL);
    g_object_get(chan2,
                 "online", &online2,
                 "name", &name2,
                 NULL);

    if(online1 && !online2)
        ret = -1;
    else if (!online1 && online2)
        ret = 1;
    else
        ret = g_strcmp0(name1, name2);

    g_free(name1);
    g_free(name2);
    g_object_unref(chan1);
    g_object_unref(chan2);

    return ret;
}

static void
channel_updating_cb(GObject* source,
                    GParamSpec* pspec,
                    gpointer udata)
{
    GtChannelsContainerFollow* self = GT_CHANNELS_CONTAINER_FOLLOW(udata);
    GtChannelsContainerFollowPrivate* priv = gt_channels_container_follow_get_instance_private(self);
    gboolean updating;

    g_object_get(source, "updating", &updating, NULL);

    if (!updating)
        gtk_flow_box_invalidate_sort(PCLASS->get_channels_flow(GT_CHANNELS_CONTAINER(self)));
}

static void
channel_followed_cb(GtFollowsManager* mgr,
                      GtChannel* chan,
                      gpointer udata)
{
    GtChannelsContainerFollow* self = GT_CHANNELS_CONTAINER_FOLLOW(udata);

    PCLASS->append_channel(GT_CHANNELS_CONTAINER(self), chan);

    PCLASS->check_empty(GT_CHANNELS_CONTAINER(self));
}

static void
channel_unfollowed_cb(GtFollowsManager* mgr,
                        GtChannel* chan,
                        gpointer udata)
{
    GtChannelsContainerFollow* self = GT_CHANNELS_CONTAINER_FOLLOW(udata);

    PCLASS->remove_channel(GT_CHANNELS_CONTAINER(self), chan);

    PCLASS->check_empty(GT_CHANNELS_CONTAINER(self));
}

static void
started_loading_follows_cb(GtFollowsManager* mgr,
                              gpointer udata)
{
    GtChannelsContainerFollow* self = GT_CHANNELS_CONTAINER_FOLLOW(udata);
    GtChannelsContainerFollowPrivate* priv = gt_channels_container_follow_get_instance_private(self);

    g_signal_handlers_block_by_func(main_app->fav_mgr, channel_followed_cb, self);

    PCLASS->clear_channels(GT_CHANNELS_CONTAINER(self));

    PCLASS->show_load_spinner(GT_CHANNELS_CONTAINER(self), TRUE);
}

static void
finished_loading_follows_cb(GtFollowsManager* mgr,
                               gpointer udata)
{
    GtChannelsContainerFollow* self = GT_CHANNELS_CONTAINER_FOLLOW(udata);
    GtChannelsContainerFollowPrivate* priv = gt_channels_container_follow_get_instance_private(self);

    PCLASS->show_load_spinner(GT_CHANNELS_CONTAINER(self), FALSE);

    PCLASS->append_channels(GT_CHANNELS_CONTAINER(self), main_app->fav_mgr->follow_channels);

    PCLASS->check_empty(GT_CHANNELS_CONTAINER(self));

    for (GList* l = main_app->fav_mgr->follow_channels; l != NULL; l = l->next)
        g_signal_connect(G_OBJECT(l->data), "notify::updating", G_CALLBACK(channel_updating_cb), self);

    g_signal_handlers_unblock_by_func(main_app->fav_mgr, channel_followed_cb, self);

}

static void
finalize(GObject* object)
{
    GtChannelsContainerFollow* self = (GtChannelsContainerFollow*) object;
    GtChannelsContainerFollowPrivate* priv = gt_channels_container_follow_get_instance_private(self);

    G_OBJECT_CLASS(gt_channels_container_follow_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtChannelsContainerFollow* self = GT_CHANNELS_CONTAINER_FOLLOW(obj);
    GtChannelsContainerFollowPrivate* priv = gt_channels_container_follow_get_instance_private(self);

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
    GtChannelsContainerFollow* self = GT_CHANNELS_CONTAINER_FOLLOW(obj);
    GtChannelsContainerFollowPrivate* priv = gt_channels_container_follow_get_instance_private(self);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_channels_container_follow_class_init(GtChannelsContainerFollowClass* klass)
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
gt_channels_container_follow_init(GtChannelsContainerFollow* self)
{
    GtChannelsContainerFollowPrivate* priv = gt_channels_container_follow_get_instance_private(self);

    gtk_flow_box_set_filter_func(PCLASS->get_channels_flow(GT_CHANNELS_CONTAINER(self)),
                                 (GtkFlowBoxFilterFunc) filter_follows_by_name,
                                 self, NULL);
    gtk_flow_box_set_sort_func(PCLASS->get_channels_flow(GT_CHANNELS_CONTAINER(self)),
                               (GtkFlowBoxSortFunc) sort_follows_by_name_and_online,
                               self, NULL);

    PCLASS->set_empty_info(GT_CHANNELS_CONTAINER(self),
                           "emblem-favorite-symbolic",
                           _("No channels followed"),
                           _("Follow channels that you like for them to show up here"));

    PCLASS->set_loading_info(GT_CHANNELS_CONTAINER(self), _("Loading follows"));

    g_signal_connect(main_app->fav_mgr, "channel-followed",
                     G_CALLBACK(channel_followed_cb), self);
    g_signal_connect(main_app->fav_mgr, "channel-unfollowed",
                     G_CALLBACK(channel_unfollowed_cb), self);
    g_signal_connect(main_app->fav_mgr, "finished-loading-follows",
                     G_CALLBACK(finished_loading_follows_cb), self);
    g_signal_connect(main_app->fav_mgr, "started-loading-follows",
                     G_CALLBACK(started_loading_follows_cb), self);

    started_loading_follows_cb(main_app->fav_mgr, self);

    if (!gt_app_credentials_valid(main_app))
        finished_loading_follows_cb(main_app->fav_mgr, self);
}
