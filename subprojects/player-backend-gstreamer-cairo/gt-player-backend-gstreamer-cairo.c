#include "gt-player-backend-gstreamer-cairo.h"
#include "gnome-twitch/gt-player-backend.h"
#include <gst/gst.h>

typedef struct
{
    GstElement* playbin;
    GstElement* video_sink;

    GtkWidget* widget;

    gchar* uri;

    gdouble volume;
    gdouble muted;
    gboolean playing;
} GtPlayerBackendGstreamerCairoPrivate;

static void gt_player_backend_iface_init(GtPlayerBackendInterface* iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(GtPlayerBackendGstreamerCairo, gt_player_backend_gstreamer_cairo, PEAS_TYPE_EXTENSION_BASE, 0,
                               G_IMPLEMENT_INTERFACE_DYNAMIC(GT_TYPE_PLAYER_BACKEND, gt_player_backend_iface_init)
                               G_ADD_PRIVATE_DYNAMIC(GtPlayerBackendGstreamerCairo))

enum
{
    PROP_0,
    PROP_VOLUME,
    PROP_PLAYING,
    PROP_MUTED,
    PROP_URI,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

static void
play(GtPlayerBackendGstreamerCairo* self)
{
    GtPlayerBackendGstreamerCairoPrivate* priv = gt_player_backend_gstreamer_cairo_get_instance_private(self);

    gst_element_set_state(priv->playbin, GST_STATE_PLAYING);
}

static void
stop(GtPlayerBackendGstreamerCairo* self)
{
    GtPlayerBackendGstreamerCairoPrivate* priv = gt_player_backend_gstreamer_cairo_get_instance_private(self);

    gst_element_set_state(priv->playbin, GST_STATE_NULL);
}

static GtkWidget*
get_widget(GtPlayerBackend* backend)
{
    GtPlayerBackendGstreamerCairo* self = GT_PLAYER_BACKEND_GSTREAMER_CAIRO(backend);
    GtPlayerBackendGstreamerCairoPrivate* priv = gt_player_backend_gstreamer_cairo_get_instance_private(self);

    return priv->widget;
}

static void
finalise(GObject* obj)
{
    GtPlayerBackendGstreamerCairo* self = GT_PLAYER_BACKEND_GSTREAMER_CAIRO(obj);
    GtPlayerBackendGstreamerCairoPrivate* priv = gt_player_backend_gstreamer_cairo_get_instance_private(self);

    g_message("{GtPlayerBackendGstreamerCairo} Finalise");

    G_OBJECT_CLASS(gt_player_backend_gstreamer_cairo_parent_class)->finalize(obj);

//    gst_object_unref(priv->video_sink);
    gst_object_unref(priv->playbin);

    g_clear_object(&priv->widget);

    g_free(priv->uri);

//    gst_deinit();
}

static void
get_property(GObject* obj,
             guint prop,
             GValue* val,
             GParamSpec* pspec)
{
    GtPlayerBackendGstreamerCairo* self = GT_PLAYER_BACKEND_GSTREAMER_CAIRO(obj);
    GtPlayerBackendGstreamerCairoPrivate* priv = gt_player_backend_gstreamer_cairo_get_instance_private(self);

    switch (prop)
    {
        case PROP_VOLUME:
            g_value_set_double(val, priv->volume);
            break;
        case PROP_MUTED:
            g_value_set_boolean(val, priv->muted);
            break;
        case PROP_PLAYING:
            g_value_set_boolean(val, priv->playing);
            break;
        case PROP_URI:
            g_value_set_string(val, priv->uri);
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
    GtPlayerBackendGstreamerCairo* self = GT_PLAYER_BACKEND_GSTREAMER_CAIRO(obj);
    GtPlayerBackendGstreamerCairoPrivate* priv = gt_player_backend_gstreamer_cairo_get_instance_private(self);

    switch (prop)
    {
        case PROP_VOLUME:
            priv->volume = g_value_get_double(val);
            break;
        case PROP_MUTED:
            priv->muted = g_value_get_boolean(val);
            break;
        case PROP_PLAYING:
            priv->playing = g_value_get_boolean(val);
            priv->playing ? play(self) : stop(self);
            break;
        case PROP_URI:
            g_free(priv->uri);
            priv->uri = g_value_dup_string(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_player_backend_iface_init(GtPlayerBackendInterface* iface)
{
    iface->get_widget = get_widget;
}

static void
gt_player_backend_gstreamer_cairo_class_finalize(GtPlayerBackendGstreamerCairoClass* klass)
{
    if (gst_is_initialized())
        gst_deinit();
}

static void
gt_player_backend_gstreamer_cairo_class_init(GtPlayerBackendGstreamerCairoClass* klass)
{
    GObjectClass* obj_class = G_OBJECT_CLASS(klass);

    obj_class->finalize = finalise;
    obj_class->get_property = get_property;
    obj_class->set_property = set_property;

    props[PROP_VOLUME] = g_param_spec_double("volume",
                                             "Volume",
                                             "Volume of player",
                                             0.0, 1.0, 0.3,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_MUTED] = g_param_spec_boolean("muted",
                                             "Muted",
                                             "Whether muted",
                                             FALSE,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_PLAYING] = g_param_spec_boolean("playing",
                                               "Playing",
                                               "Whether playing",
                                               FALSE,
                                               G_PARAM_READABLE | G_PARAM_CONSTRUCT);
    props[PROP_URI] = g_param_spec_string("uri",
                                          "Uri",
                                          "Current uri",
                                          "",
                                          G_PARAM_READWRITE);

    g_object_class_override_property(obj_class, PROP_VOLUME, "volume");
    g_object_class_override_property(obj_class, PROP_MUTED, "muted");
    g_object_class_override_property(obj_class, PROP_PLAYING, "playing");
    g_object_class_override_property(obj_class, PROP_URI, "uri");

    if (!gst_is_initialized())
        gst_init(NULL, NULL);
}

static void
gt_player_backend_gstreamer_cairo_init(GtPlayerBackendGstreamerCairo* self)
{
    GtPlayerBackendGstreamerCairoPrivate* priv = gt_player_backend_gstreamer_cairo_get_instance_private(self);

    g_message("{GtPlayerBackendGstreamerCairo} Init");

    priv->playbin = gst_element_factory_make("playbin", NULL);
    priv->video_sink = gst_element_factory_make("gtksink", NULL);

    g_object_get(priv->video_sink, "widget", &priv->widget, NULL);
    g_object_set(priv->playbin, "video-sink", priv->video_sink, NULL);

    g_object_bind_property(self, "volume",
                           priv->playbin, "volume",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(self, "muted",
                           priv->playbin, "mute",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(self, "uri",
                           priv->playbin, "uri",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
}

G_MODULE_EXPORT
void
peas_register_types(PeasObjectModule* module)
{
    gt_player_backend_gstreamer_cairo_register_type(G_TYPE_MODULE(module));

    peas_object_module_register_extension_type(module,
                                               GT_TYPE_PLAYER_BACKEND,
                                               GT_TYPE_PLAYER_BACKEND_GSTREAMER_CAIRO);
}
