#include "gt-item-container.h"
#include "utils.h"

#define TAG "GtItemContainer"
#include "gnome-twitch/gt-log.h"

typedef struct
{
    GtkWidget* item_scroll;
    GtkWidget* item_flow;
    GtkWidget* fetching_label;
    GtkWidget* empty_label;
    GtkWidget* empty_sub_label;
    GtkWidget* empty_image;

    gint child_width;
    gint child_height;
    gboolean append_extra;
    gchar* empty_label_text;
    gchar* empty_sub_label_text;
    gchar* empty_image_name;
    gchar* fetching_label_text;

    guint num_items;
    gboolean fetching_items;

    GCancellable* cancel;

    GdkRectangle* alloc;
} GtItemContainerPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(GtItemContainer, gt_item_container, GTK_TYPE_STACK);

enum
{
    PROP_0,
    PROP_FETCHING_ITEMS,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

static void
fetch_items_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    GtItemContainer* self = GT_ITEM_CONTAINER(source);
    GtItemContainerPrivate* priv = gt_item_container_get_instance_private(self);

    GError* err = NULL;

    GList* items = g_task_propagate_pointer(G_TASK(res), &err);

    //TODO: Show error message to user
    if (err)
    {
        if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            TRACE("Search was cancelled");
        else
            WARNINGF("Unable to fetch items because: %s", err->message);

        g_error_free(err);

        return;
    }

    for (GList* l = items; l != NULL; l = l->next)
    {
        gtk_container_add(GTK_CONTAINER(priv->item_flow),
            GT_ITEM_CONTAINER_GET_CLASS(self)->create_child(l->data));

        priv->num_items++;
    }

    if (priv->num_items == 0)
        gtk_stack_set_visible_child_name(GTK_STACK(self), "empty");

    gtk_widget_show_all(GTK_WIDGET(self));

    priv->fetching_items = FALSE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_FETCHING_ITEMS]);
}

static void
fetch_items(GtItemContainer* self)
{
    GtItemContainerPrivate* priv = gt_item_container_get_instance_private(self);

    GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(priv->item_scroll));

    guint height = priv->num_items == 0 ?
        gtk_widget_get_allocated_height(GTK_WIDGET(self)) :
        ROUND(gtk_adjustment_get_upper(vadj));

    guint width = gtk_widget_get_allocated_width(GTK_WIDGET(self));

    /* Haven't been allocated size yet, wait for first size allocate */
    if (height <= 1 && width <= 1)
    {
        if (!priv->append_extra)
            utils_signal_connect_oneshot(self, "size-allocate", G_CALLBACK(fetch_items), NULL);
        return;
    }

    priv->fetching_items = TRUE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_FETCHING_ITEMS]);

    gtk_stack_set_visible_child_name(GTK_STACK(self), "main");

    guint columns = width / priv->child_width;
    guint rows = height / priv->child_height;

    guint leftover = MAX(columns*rows - priv->num_items , 0);

    guint amount = leftover + columns*3;

    utils_refresh_cancellable(&priv->cancel);

    FetchItemsData* data = g_new(FetchItemsData, 1);
    data->amount = amount;
    data->offset = priv->num_items;

    GTask* task = g_task_new(self, priv->cancel, fetch_items_cb, NULL);
    g_task_set_task_data(task, data, g_free);
    g_task_set_return_on_cancel(task, FALSE);

    g_task_run_in_thread(task, GT_ITEM_CONTAINER_GET_CLASS(self)->fetch_items);

    g_object_unref(task);
}

static void
append_extra(GtkWidget* widget,
    GdkRectangle* alloc,
    gpointer udata)
{
    GtItemContainer* self = GT_ITEM_CONTAINER(udata);
    GtItemContainerPrivate* priv = gt_item_container_get_instance_private(self);

    if (priv->fetching_items)
        return;

    if (alloc->width < 10 || alloc->height < 10)
    {
        g_print("Small alloc\n");
        return;
    }

    gboolean bigger =
        alloc->width > priv->alloc->width ||
        alloc->height > priv->alloc->height;

    g_free(priv->alloc);
    priv->alloc = g_memdup(alloc, sizeof(GdkRectangle));

    if (!bigger)
        return;

    GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(priv->item_scroll));

    gboolean scroll_fills_alloc =
        gtk_adjustment_get_upper(vadj) - priv->child_height <
        alloc->height;

    gboolean near_upper =
        gtk_adjustment_get_value(vadj) + gtk_adjustment_get_page_size(vadj) +
        priv->child_height*2 > gtk_adjustment_get_upper(vadj);

    if (!scroll_fills_alloc && !near_upper)
        return;

    fetch_items(self);
}

static void
edge_reached_cb(GtkScrolledWindow* scroll,
    GtkPositionType pos, gpointer udata)
{
    g_assert(GT_IS_ITEM_CONTAINER(udata));
    g_assert(GTK_IS_SCROLLED_WINDOW(scroll));

    GtItemContainer* self = GT_ITEM_CONTAINER(udata);

    if (pos == GTK_POS_BOTTOM)
        fetch_items(self);
}

static void
child_activated_cb(GtkFlowBox* box,
    GtkFlowBoxChild* child,
    gpointer udata)
{
    GtItemContainer* self = GT_ITEM_CONTAINER(udata);

    GT_ITEM_CONTAINER_GET_CLASS(self)->activate_child(self, child);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtItemContainer* self = GT_ITEM_CONTAINER(obj);
    GtItemContainerPrivate* priv = gt_item_container_get_instance_private(self);

    switch (prop)
    {
        case PROP_FETCHING_ITEMS:
            g_value_set_boolean(val, priv->fetching_items);
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
    GtItemContainer* self = GT_ITEM_CONTAINER(obj);
    GtItemContainerPrivate* priv = gt_item_container_get_instance_private(self);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
constructed(GObject* obj)
{
    GtItemContainer* self = GT_ITEM_CONTAINER(obj);
    GtItemContainerPrivate* priv = gt_item_container_get_instance_private(self);

    //NOTE: This causes a segfault
    /* G_OBJECT_GET_CLASS(gt_item_container_parent_class)->constructed(obj); */

    GT_ITEM_CONTAINER_GET_CLASS(self)->get_properties(self,
        &priv->child_width, &priv->child_height, &priv->append_extra,
        &priv->empty_label_text, &priv->empty_sub_label_text, &priv->empty_image_name, &priv->fetching_label_text);

    gtk_label_set_text(GTK_LABEL(priv->empty_label), priv->empty_label_text);
    gtk_label_set_text(GTK_LABEL(priv->empty_sub_label), priv->empty_sub_label_text);
    gtk_label_set_text(GTK_LABEL(priv->fetching_label), priv->fetching_label_text);
    gtk_image_set_from_icon_name(GTK_IMAGE(priv->empty_image), priv->empty_image_name, GTK_ICON_SIZE_DIALOG);

    //TODO: This should probably be connected to the window's size-allocate
    if (priv->append_extra)
    {
        g_signal_connect(self, "size-allocate", G_CALLBACK(append_extra), self);
        g_signal_connect(priv->item_scroll, "edge-reached", G_CALLBACK(edge_reached_cb), self);
    }

    g_signal_connect(priv->item_flow, "child-activated", G_CALLBACK(child_activated_cb), self);
}

static void
gt_item_container_class_init(GtItemContainerClass* klass)
{
    G_OBJECT_CLASS(klass)->set_property = set_property;
    G_OBJECT_CLASS(klass)->get_property = get_property;
    G_OBJECT_CLASS(klass)->constructed = constructed;

    props[PROP_FETCHING_ITEMS] = g_param_spec_boolean(
        "fetching-items",
        "Fetching items",
        "Whether fetching items",
        FALSE,
        G_PARAM_READABLE);

    g_object_class_install_properties(G_OBJECT_CLASS(klass), NUM_PROPS, props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
        "/com/vinszent/GnomeTwitch/ui/gt-item-container.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtItemContainer, item_flow);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtItemContainer, item_scroll);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtItemContainer, empty_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtItemContainer, empty_sub_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtItemContainer, empty_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtItemContainer, fetching_label);
}

static void
gt_item_container_init(GtItemContainer* self)
{
    GtItemContainerPrivate* priv = gt_item_container_get_instance_private(self);

    priv->num_items = 0;
    priv->alloc = g_new(GdkRectangle, 1);
    priv->alloc->width = 0;
    priv->alloc->height = 0;

    gtk_widget_init_template(GTK_WIDGET(self));
}

GtkWidget*
gt_item_container_get_flow_box(GtItemContainer* self)
{
    g_assert(GT_IS_ITEM_CONTAINER(self));

    GtItemContainerPrivate* priv = gt_item_container_get_instance_private(self);

    return priv->item_flow;
}

void
gt_item_container_append_items(GtItemContainer* self, GList* items)
{
    g_print("Length %d\n", g_list_length(items));
}

//TODO: Make this overridable, mainly for GtFollowedChannelContainer
void
gt_item_container_refresh(GtItemContainer* self)
{
    GtItemContainerPrivate* priv = gt_item_container_get_instance_private(self);

    g_assert(GT_IS_ITEM_CONTAINER(self));

    DEBUG("Refreshing");

    utils_container_clear(GTK_CONTAINER(priv->item_flow));

    priv->num_items = 0;

    fetch_items(self);
}
