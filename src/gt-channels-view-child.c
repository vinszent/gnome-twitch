#include "gt-channels-view-child.h"
#include "utils.h"

typedef struct
{
    GtTwitchChannel* channel;

    GtkWidget* preview_image;
    GtkWidget* name_label;
    GtkWidget* game_label;
    GtkWidget* event_box;
    GtkWidget* middle_revealer;
    GtkWidget* viewers_label;
    GtkWidget* time_label;
    GtkWidget* favourite_button;
} GtChannelsViewChildPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtChannelsViewChild, gt_channels_view_child, GTK_TYPE_FLOW_BOX_CHILD)

enum 
{
    PROP_0,
    PROP_TWITCH_CHANNEL,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtChannelsViewChild*
gt_channels_view_child_new(GtTwitchChannel* chan)
{
    return g_object_new(GT_TYPE_CHANNELS_VIEW_CHILD, 
                        "twitch-channel", chan,
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

    gt_twitch_channel_toggle_favourited(priv->channel);
}

static void
finalize(GObject* object)
{
    GtChannelsViewChild* self = (GtChannelsViewChild*) object;
    GtChannelsViewChildPrivate* priv = gt_channels_view_child_get_instance_private(self);

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
        case PROP_TWITCH_CHANNEL:
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
        case PROP_TWITCH_CHANNEL:
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
    GdkPixbuf* preview;
    gchar* name;
    gchar* game;
    gint64 viewers;
    GDateTime* created_at;
    GDateTime* now;
    GTimeSpan dif;

    g_object_get(priv->channel, 
                 "preview", &preview, 
                 "display-name", &name,
                 "game", &game,
                 "viewers", &viewers,
                 "created-at", &created_at,
                 NULL);

    now = g_date_time_new_now_utc();
    dif = g_date_time_difference(now, created_at);

    gtk_label_set_label(GTK_LABEL(priv->name_label), name);
    gtk_label_set_label(GTK_LABEL(priv->game_label), game);
    gtk_label_set_label(GTK_LABEL(priv->viewers_label), viewers > 1e4 ? 
                        g_strdup_printf("%2.1fk", (double) viewers / 1e3) : 
                        g_strdup_printf("%ld", viewers)); //TODO: Too lazy now, but the printf needs to be freed
    gtk_label_set_label(GTK_LABEL(priv->time_label), dif > 3.6*1e9 ?
                        g_strdup_printf("%2.1fh", (double) dif / 3.6e9) :
                        g_strdup_printf("%ldm", dif / (gint64) 6e7));
    /* gtk_image_set_from_pixbuf(GTK_IMAGE(priv->preview_image), preview); */

    g_object_bind_property(priv->channel, "preview",
                           priv->preview_image, "pixbuf",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(priv->channel, "favourited",
                           priv->favourite_button, "active",
                           G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

    /* g_signal_connect(priv->channel, "notify::favourited", G_CALLBACK(channel_favourited_cb), self); */

    g_object_unref(preview);
    g_free(name);
    g_free(game);
    g_date_time_unref(now);

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

    props[PROP_TWITCH_CHANNEL] = g_param_spec_object("twitch-channel",
                                                     "Twitch channel",
                                                     "Associated channel",
                                                     GT_TYPE_TWITCH_CHANNEL,
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
