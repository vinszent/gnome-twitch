#include "gt-channels-view.h"
#include "gt-channels-container-top.h"
#include "gt-channels-container-favourite.h"
#include "gt-channels-container-search.h"
#include "gt-channels-container-game.h"

#define VISIBLE_CONTAINER GT_CHANNELS_CONTAINER(gtk_stack_get_visible_child(GTK_STACK(priv->channels_stack)))

typedef struct
{
    gboolean search_active;

    GtkWidget* channels_stack;
    GtkWidget* top_container;
    GtkWidget* search_container;
    GtkWidget* search_bar;
} GtChannelsViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtChannelsView, gt_channels_view, GTK_TYPE_BOX)

enum 
{
    PROP_0,
    PROP_SEARCH_ACTIVE,
    PROP_SHOWING_TOP_CHANNELS,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtChannelsView*
gt_channels_view_new(void)
{
    return g_object_new(GT_TYPE_CHANNELS_VIEW, 
                        NULL);
}

static void
finalize(GObject* object)
{
    GtChannelsView* self = (GtChannelsView*) object;
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    G_OBJECT_CLASS(gt_channels_view_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtChannelsView* self = GT_CHANNELS_VIEW(obj);
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    switch (prop)
    {
        case PROP_SEARCH_ACTIVE:
            g_value_set_boolean(val, priv->search_active);
            break;
        case PROP_SHOWING_TOP_CHANNELS:
            g_value_set_boolean(val, gtk_stack_get_visible_child(GTK_STACK(priv->channels_stack)) == priv->top_container);
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
    GtChannelsView* self = GT_CHANNELS_VIEW(obj);
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

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
search_changed_cb(GtkEditable* edit,
                  gpointer udata)
{
    GtChannelsView* self = GT_CHANNELS_VIEW(udata);
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    const gchar* query = gtk_entry_get_text(GTK_ENTRY(edit));

    gt_channels_container_set_filter_query(GT_CHANNELS_CONTAINER(priv->search_container), query);
}

static void
search_active_cb(GObject* source,
                 GParamSpec* pspec,
                 gpointer udata)
{
    GtChannelsView* self = GT_CHANNELS_VIEW(udata);
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    if (priv->search_active)
        gtk_stack_set_visible_child(GTK_STACK(priv->channels_stack), priv->search_container);
    else
        gtk_stack_set_visible_child(GTK_STACK(priv->channels_stack), priv->top_container);
}

static void
gt_channels_view_class_init(GtChannelsViewClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    GT_TYPE_CHANNELS_CONTAINER_TOP;
    GT_TYPE_CHANNELS_CONTAINER_SEARCH;

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_SEARCH_ACTIVE] = g_param_spec_boolean("search-active",
                                                     "Search Active",
                                                     "Whether search is active",
                                                     FALSE,
                                                     G_PARAM_READWRITE);
    props[PROP_SHOWING_TOP_CHANNELS] = g_param_spec_boolean("showing-top-channels",
                                                            "Showing Top Channels",
                                                            "Whether showing top channels",
                                                            FALSE,
                                                            G_PARAM_READABLE);
    g_object_class_install_properties(object_class,
                                      NUM_PROPS,
                                      props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), "/com/gnome-twitch/ui/gt-channels-view.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsView, channels_stack);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsView, top_container);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsView, search_container);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsView, search_bar);

    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), search_changed_cb);
}

static void
gt_channels_view_init(GtChannelsView* self)
{
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    gtk_widget_init_template(GTK_WIDGET(self));

    g_object_bind_property(self, "search-active",
                           priv->search_bar, "search-mode-enabled",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

    g_signal_connect(self, "notify::search-active", G_CALLBACK(search_active_cb), self);
}

void
gt_channels_view_refresh(GtChannelsView* self)
{
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    gt_channels_container_refresh(VISIBLE_CONTAINER);
}

void
gt_channels_view_show_type(GtChannelsView* self, GtChannelsContainerType type)
{
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    switch (type)
    {
        case GT_CHANNELS_CONTAINER_TYPE_TOP:
            gtk_stack_set_visible_child(GTK_STACK(priv->channels_stack), priv->top_container);
            break;
        case GT_CHANNELS_CONTAINER_TYPE_SEARCH:
            gtk_stack_set_visible_child(GTK_STACK(priv->channels_stack), priv->search_container);
            break;
        default:
            break;
    }

    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_SHOWING_TOP_CHANNELS]);
}

gboolean
gt_channels_view_handle_event(GtChannelsView* self, GdkEvent* event)
{
    GtChannelsViewPrivate* priv = gt_channels_view_get_instance_private(self);

    return gtk_search_bar_handle_event(GTK_SEARCH_BAR(priv->search_bar), event);
}
