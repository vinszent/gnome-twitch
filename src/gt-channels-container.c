#include "gt-channels-container.h"
#include "gt-channel.h"
#include "gt-channels-container-child.h"
#include "gt-win.h"
#include "utils.h"

typedef struct
{
    GtkWidget* channels_flow;
    GtkWidget* load_revealer;
} GtChannelsContainerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtChannelsContainer, gt_channels_container, GTK_TYPE_BOX)

enum 
{
    PROP_0,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtChannelsContainer*
gt_channels_container_new(void)
{
    return g_object_new(GT_TYPE_CHANNELS_CONTAINER, 
                        NULL);
}

static void
show_load_spinner(GtChannelsContainer* self, gboolean show)
{
    GtChannelsContainerPrivate* priv = gt_channels_container_get_instance_private(self);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->load_revealer), show);
}

static void
append_channel(GtChannelsContainer* self, GtChannel* chan)
{
    GtChannelsContainerPrivate* priv = gt_channels_container_get_instance_private(self);

    GtChannelsContainerChild* child = gt_channels_container_child_new(chan);
    gtk_widget_show_all(GTK_WIDGET(child));
    gtk_container_add(GTK_CONTAINER(priv->channels_flow), GTK_WIDGET(child));
}

static void
append_channels(GtChannelsContainer* self, GList* channels)
{
    GtChannelsContainerPrivate* priv = gt_channels_container_get_instance_private(self);

    for (GList* l = channels; l != NULL; l = l->next)
    {
        GtChannel* chan = GT_CHANNEL(l->data);

        append_channel(self, chan);
    }
}

static void
remove_channel(GtChannelsContainer* self, GtChannel* chan)
{
    GtChannelsContainerPrivate* priv = gt_channels_container_get_instance_private(self);

    for (GList* l = gtk_container_get_children(GTK_CONTAINER(priv->channels_flow)); l != NULL; l = l->next)
    {
        GtChannelsContainerChild* child = GT_CHANNELS_CONTAINER_CHILD(l->data);
        GtChannel* _chan = NULL;

        g_object_get(child, "channel", &_chan, NULL);

        if (!gt_channel_compare(chan, _chan))
        {
            g_object_unref(_chan);
            gtk_container_remove(GTK_CONTAINER(priv->channels_flow), GTK_WIDGET(child));
            break;
        }

        g_object_unref(_chan);
    }
}

static GtkFlowBox*
get_channels_flow(GtChannelsContainer* self)
{
    GtChannelsContainerPrivate* priv = gt_channels_container_get_instance_private(self);

    return GTK_FLOW_BOX(priv->channels_flow);
}

static void
edge_reached_cb(GtkScrolledWindow* scroll,
                GtkPositionType pos,
                gpointer udata)
{
    GtChannelsContainer* self = GT_CHANNELS_CONTAINER(udata);
    GtChannelsContainerPrivate* priv = gt_channels_container_get_instance_private(self);

    if (pos == GTK_POS_BOTTOM)
    {
        GT_CHANNELS_CONTAINER_GET_CLASS(self)->bottom_edge_reached(self);
    }
}

static void
child_activated_cb(GtkFlowBox* flow,
                   GtkFlowBoxChild* _child,
                   gpointer udata)
{
    GtChannelsContainer* self = GT_CHANNELS_CONTAINER(udata);
    GtChannelsContainerChild* child = GT_CHANNELS_CONTAINER_CHILD(_child);
    GtChannel* chan;
    gboolean updating = FALSE;

    if (!gtk_widget_get_sensitive(GTK_WIDGET(child)))
	return;

    gt_channels_container_child_hide_overlay(child);

    g_object_get(child, "channel", &chan, NULL);
    
    g_object_get(chan, "updating", &updating, NULL);
    
    if (!updating)
	gt_win_open_channel(GT_WIN_TOPLEVEL(GTK_WIDGET(child)), chan);

    g_object_unref(chan);
}

static void
finalize(GObject* object)
{
    GtChannelsContainer* self = (GtChannelsContainer*) object;
    GtChannelsContainerPrivate* priv = gt_channels_container_get_instance_private(self);

    G_OBJECT_CLASS(gt_channels_container_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtChannelsContainer* self = GT_CHANNELS_CONTAINER(obj);
    GtChannelsContainerPrivate* priv = gt_channels_container_get_instance_private(self);

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
    GtChannelsContainer* self = GT_CHANNELS_CONTAINER(obj);
    GtChannelsContainerPrivate* priv = gt_channels_container_get_instance_private(self);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_channels_container_class_init(GtChannelsContainerClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    klass->show_load_spinner = show_load_spinner;
    klass->append_channel = append_channel;
    klass->append_channels = append_channels;
    klass->remove_channel = remove_channel;
    klass->get_channels_flow = get_channels_flow;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), "/com/gnome-twitch/ui/gt-channels-container.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainer, channels_flow);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainer, load_revealer);

    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), edge_reached_cb);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), child_activated_cb);
}

static void
gt_channels_container_init(GtChannelsContainer* self)
{
    GtChannelsContainerPrivate* priv = gt_channels_container_get_instance_private(self);

    gtk_widget_init_template(GTK_WIDGET(self));
}

void
gt_channels_container_refresh(GtChannelsContainer* self)
{
    GtChannelsContainerPrivate* priv = gt_channels_container_get_instance_private(self);

    GT_CHANNELS_CONTAINER_GET_CLASS(self)->refresh(self);
}

void
gt_channels_container_set_filter_query(GtChannelsContainer* self, const gchar* query)
{
    GT_CHANNELS_CONTAINER_GET_CLASS(self)->filter(self, query);
}
