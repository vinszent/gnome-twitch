#include <glib/gi18n.h>
#include "gt-followed-channel-container.h"
#include "gt-item-container.h"
#include "gt-twitch.h"
#include "gt-app.h"
#include "gt-channels-container-child.h"
#include "gt-win.h"

#define TAG "GtFollowedChannelContainer"
#include "gnome-twitch/gt-log.h"

#define CHILD_WIDTH 350
#define CHILD_HEIGHT 230
#define APPEND_EXTRA FALSE

typedef struct
{
    gchar* query;
    GtkWidget* item_flow;
} GtFollowedChannelContainerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtFollowedChannelContainer, gt_followed_channel_container, GT_TYPE_ITEM_CONTAINER);

enum
{
    PROP_0,
    PROP_QUERY,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

static void
get_properties(GtItemContainer* self,
    gint* child_width, gint* child_height, gboolean* append_extra,
    gchar** empty_label_text, gchar** empty_sub_label_text, gchar** empty_image_name, gchar** fetching_label_text)
{
    *child_width = CHILD_WIDTH;
    *child_height = CHILD_HEIGHT;
    *append_extra = APPEND_EXTRA;
    *empty_label_text = g_strdup(_("No channels followed"));
    *empty_sub_label_text = g_strdup(_("Follow channels that you like for them to show up here!"));
    *empty_image_name = g_strdup("emblem-favorite-symbolic");
    *fetching_label_text = g_strdup(_("Fetching channels"));
}

static GtkWidget*
create_child(gpointer data)
{
    g_assert(GT_IS_CHANNEL(data));

    return GTK_WIDGET(gt_channels_container_child_new(GT_CHANNEL(data)));
}

static void
activate_child(GtItemContainer* item_container,
    gpointer child)
{
    g_assert(GT_IS_FOLLOWED_CHANNEL_CONTAINER(item_container));
    g_assert(GT_IS_CHANNELS_CONTAINER_CHILD(child));

    gt_win_open_channel(GT_WIN_ACTIVE,
        GT_CHANNELS_CONTAINER_CHILD(child)->channel);
}

static void
fetch_items(GTask* task,
    gpointer source, gpointer task_data,
    GCancellable* cancel)
{
    g_assert(G_IS_TASK(task));

    if (g_task_return_error_if_cancelled(task))
        return;

    FetchItemsData* data = task_data;

    g_assert_nonnull(data);

    g_task_return_pointer(task, main_app->fav_mgr->follow_channels, NULL);
}

static void
finished_loading_follows_cb(GtFollowsManager* mgr,
    gpointer udata)
{
    g_assert(GT_IS_FOLLOWED_CHANNEL_CONTAINER(udata));

    GtFollowedChannelContainer* self = GT_FOLLOWED_CHANNEL_CONTAINER(udata);
    GtFollowedChannelContainerPrivate* priv = gt_followed_channel_container_get_instance_private(self);

    gt_item_container_refresh(GT_ITEM_CONTAINER(self));
}

static void
channel_followed_cb(GtFollowsManager* mgr,
    GtChannel* chan, gpointer udata)
{
    g_assert(GT_IS_FOLLOWED_CHANNEL_CONTAINER(udata));
    g_assert(GT_IS_CHANNEL(chan));
    g_assert(GT_IS_FOLLOWS_MANAGER(mgr));

    GtFollowedChannelContainer* self = GT_FOLLOWED_CHANNEL_CONTAINER(udata);

    //TODO: Ideally we should just add the channel instead of refreshing
    // the entire container.
    gt_item_container_refresh(GT_ITEM_CONTAINER(self));
}

static void
channel_unfollowed_cb(GtFollowsManager* mgr,
    GtChannel* chan, gpointer udata)
{
    g_assert(GT_IS_FOLLOWED_CHANNEL_CONTAINER(udata));
    g_assert(GT_IS_CHANNEL(chan));
    g_assert(GT_IS_FOLLOWS_MANAGER(mgr));

    GtFollowedChannelContainer* self = GT_FOLLOWED_CHANNEL_CONTAINER(udata);

    gt_item_container_refresh(GT_ITEM_CONTAINER(self));
}

static gboolean
filter_by_name(GtkFlowBoxChild* _child,
    gpointer udata)
{
    g_assert(GT_IS_CHANNELS_CONTAINER_CHILD(_child));
    g_assert(GT_IS_FOLLOWED_CHANNEL_CONTAINER(udata));

    GtChannelsContainerChild* child = GT_CHANNELS_CONTAINER_CHILD(_child);
    GtFollowedChannelContainer* self = GT_FOLLOWED_CHANNEL_CONTAINER(udata);
    GtFollowedChannelContainerPrivate* priv = gt_followed_channel_container_get_instance_private(self);
    gboolean ret = FALSE;

    if (!priv->query)
        ret = TRUE;
    else
    {
        gchar* name = g_utf8_casefold(gt_channel_get_name(child->channel), -1);
        gchar* query = g_utf8_casefold(priv->query, -1);

        ret = g_strrstr(name, query) != NULL;

        g_free(name);
        g_free(query);
    }

    return ret;
}

static void
get_property(GObject* obj,
    guint prop,
    GValue* val,
    GParamSpec* pspec)
{
    GtFollowedChannelContainer* self = GT_FOLLOWED_CHANNEL_CONTAINER(obj);
    GtFollowedChannelContainerPrivate* priv = gt_followed_channel_container_get_instance_private(self);

    switch (prop)
    {
        case PROP_QUERY:
            g_value_set_string(val, priv->query);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
set_property(GObject* obj,
    guint prop,
    const GValue* val,
    GParamSpec* pspec)
{
    GtFollowedChannelContainer* self = GT_FOLLOWED_CHANNEL_CONTAINER(obj);
    GtFollowedChannelContainerPrivate* priv = gt_followed_channel_container_get_instance_private(self);

    switch (prop)
    {
        case PROP_QUERY:
            g_clear_pointer(&priv->query, g_free);
            priv->query = g_value_dup_string(val);

            gtk_flow_box_invalidate_filter(GTK_FLOW_BOX(priv->item_flow));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
constructed(GObject* obj)
{
    GtFollowedChannelContainer* self = GT_FOLLOWED_CHANNEL_CONTAINER(obj);
    GtFollowedChannelContainerPrivate* priv = gt_followed_channel_container_get_instance_private(self);

    G_OBJECT_CLASS(gt_followed_channel_container_parent_class)->constructed(obj);

    gtk_flow_box_set_filter_func(GTK_FLOW_BOX(priv->item_flow),
        (GtkFlowBoxFilterFunc) filter_by_name,
        self, NULL);

    //TODO: Need to refresh everytime a channel is followed or unfollowed
    g_signal_connect(main_app->fav_mgr, "finished-loading-follows", G_CALLBACK(finished_loading_follows_cb), self);
    g_signal_connect(main_app->fav_mgr, "channel-followed", G_CALLBACK(channel_followed_cb), self);
    g_signal_connect(main_app->fav_mgr, "channel-unfollowed", G_CALLBACK(channel_followed_cb), self);

    gt_item_container_refresh(GT_ITEM_CONTAINER(self));
}

static void
gt_followed_channel_container_class_init(GtFollowedChannelContainerClass* klass)
{
    G_OBJECT_CLASS(klass)->constructed = constructed;
    G_OBJECT_CLASS(klass)->get_property = get_property;
    G_OBJECT_CLASS(klass)->set_property = set_property;

    GT_ITEM_CONTAINER_CLASS(klass)->fetch_items = fetch_items;
    GT_ITEM_CONTAINER_CLASS(klass)->create_child = create_child;
    GT_ITEM_CONTAINER_CLASS(klass)->get_properties = get_properties;
    GT_ITEM_CONTAINER_CLASS(klass)->activate_child = activate_child;

    props[PROP_QUERY] = g_param_spec_string("query", "Query", "Current query", NULL, G_PARAM_READWRITE);

    g_object_class_install_properties(G_OBJECT_CLASS(klass), NUM_PROPS, props);
}

static void
gt_followed_channel_container_init(GtFollowedChannelContainer* self)
{
    GtFollowedChannelContainerPrivate* priv = gt_followed_channel_container_get_instance_private(self);

    priv->item_flow = gt_item_container_get_flow_box(GT_ITEM_CONTAINER(self));
}

GtFollowedChannelContainer*
gt_followed_channel_container_new()
{
    return g_object_new(GT_TYPE_FOLLOWED_CHANNEL_CONTAINER,
        NULL);
}
