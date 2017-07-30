/*
 *  This file is part of GNOME Twitch - 'Enjoy Twitch on your GNU/Linux desktop'
 *  Copyright © 2017 Vincent Szolnoky <vinszent@vinszent.com>
 *
 *  GNOME Twitch is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GNOME Twitch is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNOME Twitch. If not, see <http://www.gnu.org/licenses/>.
 */

#include "gt-item-container.h"
#include "utils.h"
#include "gt-win.h"
#include <glib/gi18n.h>

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
    GtkWidget* empty_box;
    GtkWidget* error_box;
    GtkWidget* error_label;
    GtkWidget* reload_button;

    gint child_width;
    gint child_height;
    gboolean append_extra;
    gchar* empty_label_text;
    gchar* empty_sub_label_text;
    gchar* empty_image_name;
    gchar* fetching_label_text;
    gchar* error_label_text;

    GHashTable* items;
    gboolean fetching_items;

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
fetch_items(GtItemContainer* self)
{
    RETURN_IF_FAIL(GT_IS_ITEM_CONTAINER(self));

    GtItemContainerPrivate* priv = gt_item_container_get_instance_private(self);

    GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(priv->item_scroll));
    gint num_items = g_hash_table_size(priv->items);

    guint height = num_items == 0 ?
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

    gtk_stack_set_visible_child(GTK_STACK(self), priv->item_scroll);

    guint columns = width / priv->child_width;
    guint rows = height / priv->child_height;

    guint leftover = MAX(columns*rows - num_items , 0);

    guint amount = leftover + columns*3;

    if (GT_ITEM_CONTAINER_GET_CLASS(self)->request_extra_items)
        GT_ITEM_CONTAINER_GET_CLASS(self)->request_extra_items(self, amount, num_items);
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
        return;

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
    RETURN_IF_FAIL(GT_IS_ITEM_CONTAINER(udata));
    RETURN_IF_FAIL(GTK_IS_SCROLLED_WINDOW(scroll));

    GtItemContainer* self = GT_ITEM_CONTAINER(udata);

    if (pos == GTK_POS_BOTTOM)
        fetch_items(self);
}

static void
child_activated_cb(GtkFlowBox* box,
    GtkFlowBoxChild* child, gpointer udata)
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
        &priv->empty_label_text, &priv->empty_sub_label_text, &priv->empty_image_name,
        &priv->error_label_text, &priv->fetching_label_text);

    gtk_label_set_text(GTK_LABEL(priv->empty_label), priv->empty_label_text);
    gtk_label_set_text(GTK_LABEL(priv->empty_sub_label), priv->empty_sub_label_text);
    gtk_label_set_text(GTK_LABEL(priv->fetching_label), priv->fetching_label_text);
    gtk_label_set_text(GTK_LABEL(priv->error_label), priv->error_label_text);
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
        "fetching-items", "Fetching items", "Whether fetching items",
        FALSE, G_PARAM_READABLE);

    g_object_class_install_properties(G_OBJECT_CLASS(klass), NUM_PROPS, props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
        "/com/vinszent/GnomeTwitch/ui/gt-item-container.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtItemContainer, item_flow);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtItemContainer, item_scroll);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtItemContainer, empty_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtItemContainer, empty_sub_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtItemContainer, empty_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtItemContainer, fetching_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtItemContainer, empty_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtItemContainer, error_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtItemContainer, error_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtItemContainer, reload_button);
}

static void
gt_item_container_init(GtItemContainer* self)
{
    GtItemContainerPrivate* priv = gt_item_container_get_instance_private(self);

    priv->items = g_hash_table_new(g_direct_hash, g_direct_equal);
    priv->alloc = g_new(GdkRectangle, 1);
    priv->alloc->width = 0;
    priv->alloc->height = 0;

    gtk_widget_init_template(GTK_WIDGET(self));

    g_signal_connect_swapped(priv->reload_button, "clicked",
        G_CALLBACK(gt_item_container_refresh), self);
}

GtkWidget*
gt_item_container_get_flow_box(GtItemContainer* self)
{
    RETURN_VAL_IF_FAIL(GT_IS_ITEM_CONTAINER(self), NULL);

    GtItemContainerPrivate* priv = gt_item_container_get_instance_private(self);

    return priv->item_flow;
}

void
gt_item_container_append_item(GtItemContainer* self, gpointer item)
{
    RETURN_IF_FAIL(GT_IS_ITEM_CONTAINER(self));
    RETURN_IF_FAIL(item != NULL);

    DEBUG("Appending item");

    GtItemContainerPrivate* priv = gt_item_container_get_instance_private(self);
    GtkWidget* child = GT_ITEM_CONTAINER_GET_CLASS(self)->create_child(self, item);

    g_hash_table_insert(priv->items, item, child);
    gtk_container_add(GTK_CONTAINER(priv->item_flow), child);

    gtk_stack_set_visible_child(GTK_STACK(self), priv->item_scroll);
}

void
gt_item_container_append_items(GtItemContainer* self, GList* items)
{
    RETURN_IF_FAIL(GT_IS_ITEM_CONTAINER(self));

    DEBUG("Appending items with list length '%d'", g_list_length(items));

    GtItemContainerPrivate* priv = gt_item_container_get_instance_private(self);

    for (GList* l = items; l != NULL; l = l->next)
    {
        GtkWidget* child = GT_ITEM_CONTAINER_GET_CLASS(self)->create_child(self, l->data);

        g_hash_table_insert(priv->items, l->data, child);
        gtk_container_add(GTK_CONTAINER(priv->item_flow), child);
    }
}

void
gt_item_container_set_items(GtItemContainer* self, GList* items)
{
    RETURN_IF_FAIL(GT_IS_ITEM_CONTAINER(self));

    DEBUG("Settings items with list length '%d'", g_list_length(items));

    GtItemContainerPrivate* priv = gt_item_container_get_instance_private(self);

    utils_container_clear(GTK_CONTAINER(priv->item_flow));

    /* NOTE: We don't call any free funcs here because all data is
     * owned by the children */
    g_hash_table_steal_all(priv->items);

    for (GList* l = items; l != NULL; l = l->next)
    {
        GtkWidget* child = GT_ITEM_CONTAINER_GET_CLASS(self)->create_child(self, l->data);

        g_hash_table_insert(priv->items, l->data, child);
        gtk_container_add(GTK_CONTAINER(priv->item_flow), child);
    }
}

void
gt_item_container_remove_item(GtItemContainer* self, gpointer item)
{
    RETURN_IF_FAIL(GT_IS_ITEM_CONTAINER(self));
    RETURN_IF_FAIL(item != NULL);

    DEBUG("Removing item");

    GtItemContainerPrivate* priv = gt_item_container_get_instance_private(self);

    GtkWidget* child = g_hash_table_lookup(priv->items, item);

    RETURN_IF_FAIL(GTK_IS_WIDGET(child));

    g_hash_table_steal(priv->items, item);

    gtk_container_remove(GTK_CONTAINER(priv->item_flow), child);

    if (g_hash_table_size(priv->items) == 0)
        gtk_stack_set_visible_child(GTK_STACK(self), priv->empty_box);
}

void
gt_item_container_set_fetching_items(GtItemContainer* self, gboolean fetching_items)
{
    RETURN_IF_FAIL(GT_IS_ITEM_CONTAINER(self));

    DEBUG("Setting fetching items to '%s'", PRINT_BOOL(fetching_items));

    GtItemContainerPrivate* priv = gt_item_container_get_instance_private(self);

    if (!fetching_items)
    {
        if (g_hash_table_size(priv->items) == 0)
            gtk_stack_set_visible_child(GTK_STACK(self), priv->empty_box);
        else
            gtk_stack_set_visible_child(GTK_STACK(self), priv->item_scroll);
    }

    priv->fetching_items = fetching_items;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_FETCHING_ITEMS]);
}

void
gt_item_container_refresh(GtItemContainer* self)
{
    RETURN_IF_FAIL(GT_IS_ITEM_CONTAINER(self));

    GtItemContainerPrivate* priv = gt_item_container_get_instance_private(self);

    DEBUG("Refreshing");

    utils_container_clear(GTK_CONTAINER(priv->item_flow));

    /* NOTE: No need to free items as they are owned by the children */
    g_hash_table_steal_all(priv->items);

    fetch_items(self);
}
