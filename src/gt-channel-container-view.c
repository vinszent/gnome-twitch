#include "gt-channel-container-view.h"
#include "gt-top-channel-container.h"
#include "gt-search-channel-container.h"
#include <glib/gi18n.h>

#define TAG "GtChannelContainerView"
#include "gnome-twitch/gt-log.h"

typedef struct
{
    GtkWidget* offline_check_button;
    GtkWidget* popover_box;
    GtkWidget* search_popover;

    GtTopChannelContainer* top_container;
    GtSearchChannelContainer* search_container;
} GtChannelContainerViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtChannelContainerView, gt_channel_container_view, GT_TYPE_CONTAINER_VIEW);

static void
search_active_cb(GObject* obj,
    GParamSpec* pspec, gpointer udata)
{
    GtChannelContainerView* self = GT_CHANNEL_CONTAINER_VIEW(udata);
    GtChannelContainerViewPrivate* priv = gt_channel_container_view_get_instance_private(self);
    GtkWidget* container_stack = gt_container_view_get_container_stack(
        GT_CONTAINER_VIEW(self));

    if (gt_container_view_get_search_active(GT_CONTAINER_VIEW(self)))
        gtk_stack_set_visible_child(GTK_STACK(container_stack), GTK_WIDGET(priv->search_container));
    else
        gtk_stack_set_visible_child(GTK_STACK(container_stack), GTK_WIDGET(priv->top_container));
}

static void
refresh(GtContainerView* view)
{
    g_assert(GT_IS_CHANNEL_CONTAINER_VIEW(view));

    GtkWidget* container = gtk_stack_get_visible_child(
        GTK_STACK(gt_container_view_get_container_stack(view)));

    g_assert(GT_IS_ITEM_CONTAINER(container));

    gt_item_container_refresh(GT_ITEM_CONTAINER(container));
}

static void
constructed(GObject* obj)
{
    GtChannelContainerView* self = GT_CHANNEL_CONTAINER_VIEW(obj);
    GtChannelContainerViewPrivate* priv = gt_channel_container_view_get_instance_private(self);
    GtkWidget* search_entry = gt_container_view_get_search_entry(GT_CONTAINER_VIEW(self));

    gt_container_view_set_search_popover_widget(GT_CONTAINER_VIEW(self), priv->popover_box);

    gt_container_view_add_container(GT_CONTAINER_VIEW(self),
        GT_ITEM_CONTAINER(priv->top_container));

    gt_container_view_add_container(GT_CONTAINER_VIEW(self),
        GT_ITEM_CONTAINER(priv->search_container));

    g_signal_connect(self, "notify::search-active", G_CALLBACK(search_active_cb), self);

    g_object_bind_property(search_entry, "text", priv->search_container, "query", G_BINDING_DEFAULT);

    G_OBJECT_CLASS(gt_channel_container_view_parent_class)->constructed(obj);
}

static void
gt_channel_container_view_class_init(GtChannelContainerViewClass* klass)
{
    G_OBJECT_CLASS(klass)->constructed = constructed;

    GT_CONTAINER_VIEW_CLASS(klass)->refresh = refresh;
}

static void
gt_channel_container_view_init(GtChannelContainerView* self)
{
    GtChannelContainerViewPrivate* priv = gt_channel_container_view_get_instance_private(self);

    priv->offline_check_button = gtk_check_button_new_with_label(_("Include offline channels"));
    priv->popover_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    g_object_set(priv->popover_box, "margin", 10, NULL);

    gtk_box_pack_start(GTK_BOX(priv->popover_box), priv->offline_check_button, FALSE, FALSE, 0);

    priv->top_container = gt_top_channel_container_new();
    priv->search_container = gt_search_channel_container_new();

    g_object_bind_property(priv->offline_check_button, "active",
        priv->search_container, "search-offline", G_BINDING_DEFAULT);
}
