#include "gt-player-backend-gstreamer-opengl.h"
#include "gnome-twitch/gt-player-backend.h"
#include <gst/gst.h>

typedef struct
{
    GstElement* playbin;
    GstElement* upload;
    GstElement* video_sink;
    GstElement* video_bin;
    GstPad* pad;
    GstPad* ghost_pad;

    GtkWidget* widget;

    gchar* uri;

    gdouble volume;
    gdouble muted;
    gboolean playing;
} GtPlayerBackendGstreamerOpenGLPrivate;

static void gt_player_backend_iface_init(GtPlayerBackendInterface* iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(GtPlayerBackendGstreamerOpenGL, gt_player_backend_gstreamer_opengl, PEAS_TYPE_EXTENSION_BASE, 0,
                               G_IMPLEMENT_INTERFACE_DYNAMIC(GT_TYPE_PLAYER_BACKEND, gt_player_backend_iface_init)
                               G_ADD_PRIVATE_DYNAMIC(GtPlayerBackendGstreamerOpenGL))

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
play(GtPlayerBackendGstreamerOpenGL* self)
{
    GtPlayerBackendGstreamerOpenGLPrivate* priv = gt_player_backend_gstreamer_opengl_get_instance_private(self);

    gst_element_set_state(priv->playbin, GST_STATE_PLAYING);
}

static void
stop(GtPlayerBackendGstreamerOpenGL* self)
{
    GtPlayerBackendGstreamerOpenGLPrivate* priv = gt_player_backend_gstreamer_opengl_get_instance_private(self);

    gst_element_set_state(priv->playbin, GST_STATE_NULL);
}

static GtkWidget*
get_widget(GtPlayerBackend* backend)
{
    GtPlayerBackendGstreamerOpenGL* self = GT_PLAYER_BACKEND_GSTREAMER_OPENGL(backend);
    GtPlayerBackendGstreamerOpenGLPrivate* priv = gt_player_backend_gstreamer_opengl_get_instance_private(self);

    return priv->widget;
}

static void
finalise(GObject* obj)
{
    GtPlayerBackendGstreamerOpenGL* self = GT_PLAYER_BACKEND_GSTREAMER_OPENGL(obj);
    GtPlayerBackendGstreamerOpenGLPrivate* priv = gt_player_backend_gstreamer_opengl_get_instance_private(self);

    g_message("{GtPlayerBackendGstreamerOpenGL} Finalise");

    G_OBJECT_CLASS(gt_player_backend_gstreamer_opengl_parent_class)->finalize(obj);

    gst_object_unref(priv->playbin);
//    gst_object_unref(priv->ghost_pad);
//    gst_object_unref(priv->pad);
//    gst_object_unref(priv->video_bin);
//    gst_object_unref(priv->video_sink);
//    gst_object_unref(priv->upload);

    g_clear_object(&priv->widget);

    g_free(priv->uri);
}

static void
get_property(GObject* obj,
             guint prop,
             GValue* val,
             GParamSpec* pspec)
{
    GtPlayerBackendGstreamerOpenGL* self = GT_PLAYER_BACKEND_GSTREAMER_OPENGL(obj);
    GtPlayerBackendGstreamerOpenGLPrivate* priv = gt_player_backend_gstreamer_opengl_get_instance_private(self);

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
    GtPlayerBackendGstreamerOpenGL* self = GT_PLAYER_BACKEND_GSTREAMER_OPENGL(obj);
    GtPlayerBackendGstreamerOpenGLPrivate* priv = gt_player_backend_gstreamer_opengl_get_instance_private(self);

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
gt_player_backend_gstreamer_opengl_class_finalize(GtPlayerBackendGstreamerOpenGLClass* klass)
{
    if (gst_is_initialized())
        gst_deinit();
}

static void
gt_player_backend_gstreamer_opengl_class_init(GtPlayerBackendGstreamerOpenGLClass* klass)
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
gt_player_backend_gstreamer_opengl_init(GtPlayerBackendGstreamerOpenGL* self)
{
    GtPlayerBackendGstreamerOpenGLPrivate* priv = gt_player_backend_gstreamer_opengl_get_instance_private(self);

    g_message("{GtPlayerBackendGstreamerOpenGL} Init");

    priv->playbin = gst_element_factory_make("playbin", NULL);
    priv->video_sink = gst_element_factory_make("gtkglsink", NULL);
    priv->video_bin = gst_bin_new("video_bin");
    priv->upload = gst_element_factory_make("glupload", NULL);

    gst_bin_add_many(GST_BIN(priv->video_bin), priv->upload, priv->video_sink, NULL);
    gst_element_link_many(priv->upload, priv->video_sink, NULL);

    priv->pad = gst_element_get_static_pad(priv->upload, "sink");
    priv->ghost_pad = gst_ghost_pad_new("sink", priv->pad);
    gst_pad_set_active(priv->ghost_pad, TRUE);
    gst_element_add_pad(priv->video_bin, priv->ghost_pad);

    g_object_get(priv->video_sink, "widget", &priv->widget, NULL);
    g_object_set(priv->playbin, "video-sink", priv->video_bin, NULL);

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
    gt_player_backend_gstreamer_opengl_register_type(G_TYPE_MODULE(module));

    peas_object_module_register_extension_type(module,
                                               GT_TYPE_PLAYER_BACKEND,
                                               GT_TYPE_PLAYER_BACKEND_GSTREAMER_OPENGL);
}
