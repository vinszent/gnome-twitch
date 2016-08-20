#include "gt-follows-view.h"
#include "gt-channels-container-follow.h"

typedef struct
{
    gboolean search_active;

    GtkWidget* follow_container;
    GtkWidget* search_bar;
} GtFollowsViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtFollowsView, gt_follows_view, GTK_TYPE_BOX)

enum
{
    PROP_0,
    PROP_SEARCH_ACTIVE,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtFollowsView*
gt_follows_view_new(void)
{
    return g_object_new(GT_TYPE_FOLLOWS_VIEW,
                        NULL);
}

static void
search_changed_cb(GtkEditable* edit,
                  gpointer udata)
{
    GtFollowsView* self = GT_FOLLOWS_VIEW(udata);
    GtFollowsViewPrivate* priv = gt_follows_view_get_instance_private(self);

    const gchar* query = gtk_entry_get_text(GTK_ENTRY(edit));

    gt_channels_container_set_filter_query(GT_CHANNELS_CONTAINER(priv->follow_container), query);
}

static void
finalize(GObject* object)
{
    GtFollowsView* self = (GtFollowsView*) object;
    GtFollowsViewPrivate* priv = gt_follows_view_get_instance_private(self);

    G_OBJECT_CLASS(gt_follows_view_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtFollowsView* self = GT_FOLLOWS_VIEW(obj);
    GtFollowsViewPrivate* priv = gt_follows_view_get_instance_private(self);

    switch (prop)
    {
        case PROP_SEARCH_ACTIVE:
            g_value_set_boolean(val, priv->search_active);
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
    GtFollowsView* self = GT_FOLLOWS_VIEW(obj);
    GtFollowsViewPrivate* priv = gt_follows_view_get_instance_private(self);

    switch (prop)
    {
        case PROP_SEARCH_ACTIVE:
            priv->search_active = g_value_get_boolean(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_follows_view_class_init(GtFollowsViewClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    GT_TYPE_CHANNELS_CONTAINER_FOLLOW;

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_SEARCH_ACTIVE] = g_param_spec_boolean("search-active",
                                                     "Search Active",
                                                     "Whether search is active",
                                                     FALSE,
                                                     G_PARAM_READWRITE);

    g_object_class_install_properties(object_class,
                                      NUM_PROPS,
                                      props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), "/com/vinszent/GnomeTwitch/ui/gt-follows-view.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtFollowsView, follow_container);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtFollowsView, search_bar);

    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), search_changed_cb);
}

static void
gt_follows_view_init(GtFollowsView* self)
{
    GtFollowsViewPrivate* priv = gt_follows_view_get_instance_private(self);

    gtk_widget_init_template(GTK_WIDGET(self));

    g_object_bind_property(self, "search-active",
                           priv->search_bar, "search-mode-enabled",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
}

gboolean
gt_follows_view_handle_event(GtFollowsView* self, GdkEvent* event)
{
    GtFollowsViewPrivate* priv = gt_follows_view_get_instance_private(self);

    return gtk_search_bar_handle_event(GTK_SEARCH_BAR(priv->search_bar), event);
}
