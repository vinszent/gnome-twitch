#include "gt-container-view.h"
#include "gt-item-container.h"

#define TAG "GtContainerView"
#include "gnome-twitch/gt-log.h"

typedef struct
{
    gboolean search_active;
    gboolean show_back_button;

    GtkWidget* search_bar;
    GtkWidget* search_entry;
    GtkWidget* menu_button;
    GtkWidget* container_stack;
} GtContainerViewPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(GtContainerView, gt_container_view, GTK_TYPE_BOX);

enum
{
    PROP_0,
    PROP_SEARCH_ACTIVE,
    PROP_SHOW_BACK_BUTTON,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

static void
get_property(GObject* obj,
    guint prop,
    GValue* val,
    GParamSpec* pspec)
{
    GtContainerView* self = GT_CONTAINER_VIEW(obj);
    GtContainerViewPrivate* priv = gt_container_view_get_instance_private(self);

    switch (prop)
    {
        case PROP_SEARCH_ACTIVE:
            g_value_set_boolean(val, priv->search_active);
            break;
        case PROP_SHOW_BACK_BUTTON:
            g_value_set_boolean(val, priv->show_back_button);
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
    GtContainerView* self = GT_CONTAINER_VIEW(obj);
    GtContainerViewPrivate* priv = gt_container_view_get_instance_private(self);

    switch (prop)
    {
        case PROP_SEARCH_ACTIVE:
            priv->search_active = g_value_get_boolean(val);
            break;
        case PROP_SHOW_BACK_BUTTON:
            priv->show_back_button = g_value_get_boolean(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}


static void
gt_container_view_class_init(GtContainerViewClass* klass)
{
    G_OBJECT_CLASS(klass)->get_property = get_property;
    G_OBJECT_CLASS(klass)->set_property = set_property;

    props[PROP_SHOW_BACK_BUTTON] = g_param_spec_boolean(
        "show-back-button",
        "Show back button",
        "Whether showing back button",
        FALSE,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    props[PROP_SEARCH_ACTIVE] = g_param_spec_boolean(
        "search-active",
        "Search active",
        "Whether search active",
        FALSE,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    g_object_class_install_properties(G_OBJECT_CLASS(klass), NUM_PROPS, props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
        "/com/vinszent/GnomeTwitch/ui/gt-container-view.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtContainerView, container_stack);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtContainerView, search_bar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtContainerView, search_entry);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtContainerView, menu_button);
}

static void
gt_container_view_init(GtContainerView* self)
{
    GtContainerViewPrivate* priv = gt_container_view_get_instance_private(self);

    gtk_widget_init_template(GTK_WIDGET(self));

    gtk_search_bar_connect_entry(GTK_SEARCH_BAR(priv->search_bar),
        GTK_ENTRY(priv->search_entry));

    g_object_bind_property(self, "search-active",
        priv->search_bar, "search-mode-enabled",
        G_BINDING_DEFAULT);
}

/* Should only be used by children */
void
gt_container_view_add_container(GtContainerView* self, GtItemContainer* container)
{
    GtContainerViewPrivate* priv = gt_container_view_get_instance_private(self);

    gtk_container_add(GTK_CONTAINER(priv->container_stack),
        GTK_WIDGET(container));

    gtk_widget_show_all(GTK_WIDGET(container));
}

void
gt_container_view_set_search_popover_widget(GtContainerView* self, GtkWidget* widget)
{
    g_assert(GT_IS_CONTAINER_VIEW(self));
    g_assert(GTK_IS_WIDGET(widget));

    GtContainerViewPrivate* priv = gt_container_view_get_instance_private(self);
    GtkWidget* popover = gtk_popover_new(priv->menu_button);

    gtk_container_add(GTK_CONTAINER(popover), widget);

    gtk_menu_button_set_popover(GTK_MENU_BUTTON(priv->menu_button), GTK_WIDGET(popover));

    gtk_widget_show_all(priv->menu_button);
    gtk_widget_show_all(widget);
}

void
gt_container_view_refresh(GtContainerView* self)
{
    g_assert(GT_IS_CONTAINER_VIEW(self));
    g_assert_nonnull(GT_CONTAINER_VIEW_GET_CLASS(self)->refresh);

    GT_CONTAINER_VIEW_GET_CLASS(self)->refresh(self);
}

void
gt_container_view_go_back(GtContainerView* self)
{
    g_assert(GT_IS_CONTAINER_VIEW(self));
    g_assert_nonnull(GT_CONTAINER_VIEW_GET_CLASS(self)->go_back);

    GT_CONTAINER_VIEW_GET_CLASS(self)->go_back(self);
}

void
gt_container_view_set_search_active(GtContainerView* self, gboolean search_active)
{
    g_assert(GT_IS_CONTAINER_VIEW(self));

    GtContainerViewPrivate* priv = gt_container_view_get_instance_private(self);

    priv->search_active = search_active;
}

gboolean
gt_container_view_get_search_active(GtContainerView* self)
{
    g_assert(GT_IS_CONTAINER_VIEW(self));

    GtContainerViewPrivate* priv = gt_container_view_get_instance_private(self);

    return priv->search_active;
}

gboolean
gt_container_view_get_show_back_button(GtContainerView* self)
{
    g_assert(GT_IS_CONTAINER_VIEW(self));

    GtContainerViewPrivate* priv = gt_container_view_get_instance_private(self);

    return priv->show_back_button;
}

GtkWidget*
gt_container_view_get_container_stack(GtContainerView* self)
{
    GtContainerViewPrivate* priv = gt_container_view_get_instance_private(self);

    g_assert(GT_IS_CONTAINER_VIEW(self));

    return priv->container_stack;
}

GtkWidget*
gt_container_view_get_search_entry(GtContainerView* self)
{
    GtContainerViewPrivate* priv = gt_container_view_get_instance_private(self);

    g_assert(GT_IS_CONTAINER_VIEW(self));

    return priv->search_entry;
}

GtkWidget*
gt_container_view_get_search_bar(GtContainerView* self)
{
    g_assert(GT_IS_CONTAINER_VIEW(self));

    GtContainerViewPrivate* priv = gt_container_view_get_instance_private(self);

    return priv->search_bar;
}
