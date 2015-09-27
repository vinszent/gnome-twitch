#include "gt-channels-view-child.h"
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

    GBinding* preview_binding;
    GBinding* viewers_binding;
    GBinding* time_online_binding;

    guint online_cb_id;
} GtChannelsViewChildPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtChannelsViewChild, gt_channels_view_child, GTK_TYPE_FLOW_BOX_CHILD)

enum 
{
    PROP_0,
    PROP_CHANNEL,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtChannelsViewChild*
gt_channels_view_child_new(GtChannel* chan)
{
    return g_object_new(GT_TYPE_CHANNELS_VIEW_CHILD, 
                        "channel", chan,
                        NULL);
}

static void
motion_enter_cb(GtkWidget* widget,
                GdkEvent* evt,
                gpointer udata)
{
    GtChannelsViewChild* self = GT_CHANNELS_VIEW_CHILD(udata);
    GtChannelsViewChildPrivate* priv = gt_channels_view_child_get_instance_private(self);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->middle_revealer), TRUE);
}

static void
motion_leave_cb(GtkWidget* widget,
                GdkEvent* evt,
                gpointer udata)
{
    GtChannelsViewChild* self = GT_CHANNELS_VIEW_CHILD(udata);
    GtChannelsViewChildPrivate* priv = gt_channels_view_child_get_instance_private(self);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->middle_revealer), FALSE);
}

static void
favourite_button_cb(GtkButton* button,
                    gpointer udata)
{
    GtChannelsViewChild* self = GT_CHANNELS_VIEW_CHILD(udata);
    GtChannelsViewChildPrivate* priv = gt_channels_view_child_get_instance_private(self);

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

    viewers = g_value_get_int64(from);

    if (viewers > 1e4)
        label = g_strdup_printf("%2.1fk", (gdouble) viewers / 1e3);
    else
        label = g_strdup_printf("%ld", viewers);

    g_value_set_string(to, label);
    g_free(label);
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


static void
online_cb(GObject* src,
          GParamSpec* pspec,
          gpointer udata)
{
    GtChannelsViewChild* self = GT_CHANNELS_VIEW_CHILD(udata);
    GtChannelsViewChildPrivate* priv = gt_channels_view_child_get_instance_private(self);
    gboolean online;

    g_object_get(priv->channel, "online", &online, NULL);

    if (priv->preview_binding)
        g_object_unref(priv->preview_binding);
    if (priv->viewers_binding)
        g_object_unref(priv->viewers_binding);
    if (priv->time_online_binding)
        g_object_unref(priv->time_online_binding);

    if (online)
    {

    }
    else
    {
        priv->preview_binding = g_object_bind_property(priv->channel, "video-banner",
                                                       priv->preview_image, "pixbuf",
                                                       G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    }
}

static void
finalize(GObject* object)
{
    GtChannelsViewChild* self = (GtChannelsViewChild*) object;
    GtChannelsViewChildPrivate* priv = gt_channels_view_child_get_instance_private(self);

    /* g_signal_handler_disconnect(priv->channel, priv->online_cb_id); */
    g_object_unref(priv->channel);

    G_OBJECT_CLASS(gt_channels_view_child_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtChannelsViewChild* self = GT_CHANNELS_VIEW_CHILD(obj);
    GtChannelsViewChildPrivate* priv = gt_channels_view_child_get_instance_private(self);

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
    GtChannelsViewChild* self = GT_CHANNELS_VIEW_CHILD(obj);
    GtChannelsViewChildPrivate* priv = gt_channels_view_child_get_instance_private(self);

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
    GtChannelsViewChild* self = GT_CHANNELS_VIEW_CHILD(obj);
    GtChannelsViewChildPrivate* priv = gt_channels_view_child_get_instance_private(self);

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

    /* priv->online_cb_id = g_signal_connect(priv->channel, "notify::online", G_CALLBACK(online_cb), self); */
    /* online_cb(NULL, NULL, self); */

    G_OBJECT_CLASS(gt_channels_view_child_parent_class)->constructed(obj);
}

static void
gt_channels_view_child_class_init(GtChannelsViewChildClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;
    object_class->constructed = constructed;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), 
                                                "/com/gnome-twitch/ui/gt-channels-view-child.ui");

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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsViewChild, preview_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsViewChild, name_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsViewChild, game_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsViewChild, event_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsViewChild, middle_revealer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsViewChild, viewers_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsViewChild, time_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsViewChild, favourite_button);
}

static void
gt_channels_view_child_init(GtChannelsViewChild* self)
{
    GtChannelsViewChildPrivate* priv = gt_channels_view_child_get_instance_private(self);

    gtk_widget_init_template(GTK_WIDGET(self));
}

void
gt_channels_view_child_hide_overlay(GtChannelsViewChild* self)
{
    GtChannelsViewChildPrivate* priv = gt_channels_view_child_get_instance_private(self);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->middle_revealer), FALSE);
}
