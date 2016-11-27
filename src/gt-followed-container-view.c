#include "gt-followed-container-view.h"
#include "gt-followed-channel-container.h"

#define TAG "GtFollowedContainerView"
#include "gnome-twitch/gt-log.h"

typedef struct
{
    GtFollowedChannelContainer* followed_container;
} GtFollowedContainerViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtFollowedContainerView, gt_followed_container_view, GT_TYPE_CONTAINER_VIEW);

static void
refresh(GtContainerView* view)
{
    g_assert(GT_IS_FOLLOWED_CONTAINER_VIEW(view));

    GtFollowedContainerView* self = GT_FOLLOWED_CONTAINER_VIEW(view);
    GtFollowedContainerViewPrivate* priv = gt_followed_container_view_get_instance_private(self);

    GtkWidget* container = gtk_stack_get_visible_child(
        GTK_STACK(gt_container_view_get_container_stack(view)));

    g_assert(GT_IS_ITEM_CONTAINER(container));

    gt_item_container_refresh(GT_ITEM_CONTAINER(container));
}

static void
constructed(GObject* obj)
{
    GtFollowedContainerView* self = GT_FOLLOWED_CONTAINER_VIEW(obj);
    GtFollowedContainerViewPrivate* priv = gt_followed_container_view_get_instance_private(self);
    GtkWidget* search_entry = gt_container_view_get_search_entry(GT_CONTAINER_VIEW(self));

    gt_container_view_add_container(GT_CONTAINER_VIEW(self),
        GT_ITEM_CONTAINER(priv->followed_container));

    g_object_bind_property(search_entry, "text", priv->followed_container, "query", G_BINDING_DEFAULT);

    G_OBJECT_CLASS(gt_followed_container_view_parent_class)->constructed(obj);
}

static void
gt_followed_container_view_class_init(GtFollowedContainerViewClass* klass)
{
    G_OBJECT_CLASS(klass)->constructed = constructed;

    GT_CONTAINER_VIEW_CLASS(klass)->refresh = refresh;
}

static void
gt_followed_container_view_init(GtFollowedContainerView* self)
{
    GtFollowedContainerViewPrivate* priv = gt_followed_container_view_get_instance_private(self);

    priv->followed_container = gt_followed_channel_container_new();
}
