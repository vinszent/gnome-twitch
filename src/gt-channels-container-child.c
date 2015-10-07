#include "gt-channels-container-child.h"
#include "utils.h"

typedef struct
{
    GtChannel* channel;

    GtkWidget* preview_image;
    GtkWidget* name_label;
    GtkWidget* game_label;
    GtkWidget* event_box;
    GtkWidget* middle_revealer;
    GtkWidget* viewers_label;
    GtkWidget* time_label;
    GtkWidget* favourite_button;
    GtkWidget* middle_stack;
} GtChannelsContainerChildPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtChannelsContainerChild, gt_channels_container_child, GTK_TYPE_FLOW_BOX_CHILD)

enum 
{
    PROP_0,
    PROP_CHANNEL,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtChannelsContainerChild*
gt_channels_container_child_new(GtChannel* chan)
{
    return g_object_new(GT_TYPE_CHANNELS_CONTAINER_CHILD, 
                        "channel", chan,
                        NULL);
}

static gboolean
updating_converter(GBinding* bind,
                   const GValue* from,
                   GValue* to,
                   gpointer udata)
{
    if (g_value_get_boolean(from))
        g_value_set_string(to, "spinner");
    else
        g_value_set_string(to, "content");

    return TRUE;
}

static void
motion_enter_cb(GtkWidget* widget,
                GdkEvent* evt,
                gpointer udata)
{
    GtChannelsContainerChild* self = GT_CHANNELS_CONTAINER_CHILD(udata);
    GtChannelsContainerChildPrivate* priv = gt_channels_container_child_get_instance_private(self);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->middle_revealer), TRUE);
}

static void
motion_leave_cb(GtkWidget* widget,
                GdkEvent* evt,
                gpointer udata)
{
    GtChannelsContainerChild* self = GT_CHANNELS_CONTAINER_CHILD(udata);
    GtChannelsContainerChildPrivate* priv = gt_channels_container_child_get_instance_private(self);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->middle_revealer), FALSE);
}

static void
favourite_button_cb(GtkButton* button,
                    gpointer udata)
{
    GtChannelsContainerChild* self = GT_CHANNELS_CONTAINER_CHILD(udata);
    GtChannelsContainerChildPrivate* priv = gt_channels_container_child_get_instance_private(self);

    gt_channel_toggle_favourited(priv->channel);
}

static void
viewers_converter(GBinding* bind,
                  const GValue* from,
                  GValue* to,
                  gpointer udata)
{
    gint64 viewers;
    gchar* label;

    if (g_value_get_int64(from) > -1)
    {
        viewers = g_value_get_int64(from);

        if (viewers > 1e4)
            label = g_strdup_printf("%2.1fk", (gdouble) viewers / 1e3);
        else
            label = g_strdup_printf("%ld", viewers);

        g_value_set_string(to, label);
        g_free(label);
    }
    else
    {
        g_value_set_string(to, NULL);
    }
}

static void
time_converter(GBinding* bind,
               const GValue* from,
               GValue* to,
               gpointer udata)
{
    gchar* label;
    GDateTime* now_time;
    GDateTime* stream_started_time;
    GTimeSpan dif;

    if (g_value_get_pointer(from) != NULL)
    {
        now_time = g_date_time_new_now_utc();
        stream_started_time = (GDateTime*) g_value_get_pointer(from);

        dif = g_date_time_difference(now_time, stream_started_time);

        if (dif > G_TIME_SPAN_HOUR)
            label =g_strdup_printf("%2.1fh", (gdouble) dif / G_TIME_SPAN_HOUR);
        else
            label = g_strdup_printf("%ldm", dif / G_TIME_SPAN_MINUTE);

        g_value_set_string(to, label);
        g_free(label);
        g_date_time_unref(now_time);
    }
    else
    {
        g_value_set_string(to, NULL);
    }
}


static void
finalize(GObject* object)
{
    GtChannelsContainerChild* self = (GtChannelsContainerChild*) object;
    GtChannelsContainerChildPrivate* priv = gt_channels_container_child_get_instance_private(self);

    g_object_unref(priv->channel);

    G_OBJECT_CLASS(gt_channels_container_child_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtChannelsContainerChild* self = GT_CHANNELS_CONTAINER_CHILD(obj);
    GtChannelsContainerChildPrivate* priv = gt_channels_container_child_get_instance_private(self);

    switch (prop)
    {
        case PROP_CHANNEL:
            g_value_set_object(val, priv->channel);
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
    GtChannelsContainerChild* self = GT_CHANNELS_CONTAINER_CHILD(obj);
    GtChannelsContainerChildPrivate* priv = gt_channels_container_child_get_instance_private(self);

    switch (prop)
    {
        case PROP_CHANNEL:
            priv->channel = g_value_ref_sink_object(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
constructed(GObject* obj)
{
    GtChannelsContainerChild* self = GT_CHANNELS_CONTAINER_CHILD(obj);
    GtChannelsContainerChildPrivate* priv = gt_channels_container_child_get_instance_private(self);

    g_object_bind_property(priv->channel, "display-name",
                           priv->name_label, "label",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(priv->channel, "game",
                           priv->game_label, "label",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(priv->channel, "favourited",
                           priv->favourite_button, "active",
                           G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
    g_object_bind_property(priv->channel, "preview",
                           priv->preview_image, "pixbuf",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(priv->channel, "online",
                           self, "sensitive",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property_full(priv->channel, "viewers",
                                priv->viewers_label, "label",
                                G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
                                (GBindingTransformFunc) viewers_converter,
                                NULL, NULL, NULL);
    g_object_bind_property_full(priv->channel, "stream-started-time",
                                priv->time_label, "label",
                                G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
                                (GBindingTransformFunc) time_converter,
                                NULL, NULL, NULL);
    g_object_bind_property_full(priv->channel, "updating",
                                priv->middle_stack, "visible-child-name",
                                G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
                                (GBindingTransformFunc) updating_converter,
                                NULL, NULL, NULL);

    G_OBJECT_CLASS(gt_channels_container_child_parent_class)->constructed(obj);
}

static void
gt_channels_container_child_class_init(GtChannelsContainerChildClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;
    object_class->constructed = constructed;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), 
                                                "/com/gnome-twitch/ui/gt-channels-container-child.ui");

    props[PROP_CHANNEL] = g_param_spec_object("channel",
                                              "Channel",
                                              "Associated channel",
                                              GT_TYPE_CHANNEL,
                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties(object_class,
                                      NUM_PROPS,
                                      props);

    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), motion_enter_cb);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), motion_leave_cb);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), favourite_button_cb);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, preview_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, name_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, game_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, event_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, middle_revealer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, viewers_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, time_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, favourite_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, middle_stack);
}

static void
gt_channels_container_child_init(GtChannelsContainerChild* self)
{
    GtChannelsContainerChildPrivate* priv = gt_channels_container_child_get_instance_private(self);

    gtk_widget_init_template(GTK_WIDGET(self));
}

void
gt_channels_container_child_hide_overlay(GtChannelsContainerChild* self)
{
    GtChannelsContainerChildPrivate* priv = gt_channels_container_child_get_instance_private(self);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->middle_revealer), FALSE);
}
