#include "gt-browse-header-bar.h"
#include "gt-win.h"

#define TAG "GtBrowseHeaderBar"
#include "utils.h"

typedef struct
{
    gboolean search_active;
    gboolean show_back_button;

    GtkWidget* back_button_revealer;
    GtkWidget* search_button;
    GtkWidget* refresh_button;
    GtkWidget* refresh_revealer;
    GtkWidget* search_revealer;
} GtBrowseHeaderBarPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtBrowseHeaderBar, gt_browse_header_bar, GTK_TYPE_HEADER_BAR)

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
    GtBrowseHeaderBar* self = GT_BROWSE_HEADER_BAR(obj);
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self);

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
    GtBrowseHeaderBar* self = GT_BROWSE_HEADER_BAR(obj);
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self);

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
constructed(GObject* obj)
{
    GtBrowseHeaderBar* self = GT_BROWSE_HEADER_BAR(obj);
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self);

    g_object_bind_property(priv->search_button, "active",
        self, "search-active",
        G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

    g_object_bind_property(priv->back_button_revealer, "reveal-child",
        self, "show-back-button",
        G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

    G_OBJECT_CLASS(gt_browse_header_bar_parent_class)->constructed(obj);
}

static void
gt_browse_header_bar_class_init(GtBrowseHeaderBarClass* klass)
{

    G_OBJECT_CLASS(klass)->constructed = constructed;
    G_OBJECT_CLASS(klass)->get_property = get_property;
    G_OBJECT_CLASS(klass)->set_property = set_property;

    props[PROP_SEARCH_ACTIVE] = g_param_spec_boolean("search-active", "Search active",
        "Whether search is active", FALSE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    props[PROP_SHOW_BACK_BUTTON] = g_param_spec_boolean("show-back-button", "Show back button",
        "Whether showing back button", FALSE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    g_object_class_install_properties(G_OBJECT_CLASS(klass), NUM_PROPS, props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
        "/com/vinszent/GnomeTwitch/ui/gt-browse-header-bar.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtBrowseHeaderBar, back_button_revealer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtBrowseHeaderBar, search_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtBrowseHeaderBar, refresh_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtBrowseHeaderBar, refresh_revealer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtBrowseHeaderBar, search_revealer);
}

static void
gt_browse_header_bar_init(GtBrowseHeaderBar* self)
{
    gtk_widget_init_template(GTK_WIDGET(self));
}

void
gt_browse_header_bar_stop_search(GtBrowseHeaderBar* self)
{
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->search_button), FALSE);
}

void
gt_browse_header_bar_toggle_search(GtBrowseHeaderBar* self)
{
    GtBrowseHeaderBarPrivate* priv = gt_browse_header_bar_get_instance_private(self);
    gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->search_button));

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->search_button), !active);
}
