#include "gt-player-backend-gstreamer-clutter.h"
#include "gnome-twitch/gt-player-backend.h"
#include <clutter-gst/clutter-gst.h>
#include <clutter-gtk/clutter-gtk.h>

typedef struct
{
    ClutterGstPlayback* player;
    ClutterActor* video_actor;
    ClutterActor* stage;
    ClutterContent* content;

    GtkWidget* widget;

    gchar* uri;

    gdouble volume;
    gdouble muted;
    gboolean playing;
    gdouble buffer_fill;
} GtPlayerBackendGstreamerClutterPrivate;

static void gt_player_backend_iface_init(GtPlayerBackendInterface* iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(GtPlayerBackendGstreamerClutter, gt_player_backend_gstreamer_clutter, PEAS_TYPE_EXTENSION_BASE, 0,
                               G_IMPLEMENT_INTERFACE_DYNAMIC(GT_TYPE_PLAYER_BACKEND, gt_player_backend_iface_init)
                               G_ADD_PRIVATE_DYNAMIC(GtPlayerBackendGstreamerClutter))

enum
{
    PROP_0,
    PROP_VOLUME,
    PROP_PLAYING,
    PROP_MUTED,
    PROP_URI,
    PROP_BUFFER_FILL,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

static void
size_allocate_cb(GtkWidget* widget,
                 GdkRectangle* alloc,
                 gpointer udata)
{
    GtPlayerBackendGstreamerClutter* self = GT_PLAYER_BACKEND_GSTREAMER_CLUTTER(udata);
    GtPlayerBackendGstreamerClutterPrivate* priv = gt_player_backend_gstreamer_clutter_get_instance_private(self);

    clutter_actor_set_width(priv->video_actor, (gfloat) alloc->width);
    clutter_actor_set_height(priv->video_actor, (gfloat) alloc->height);
}

static void
play(GtPlayerBackendGstreamerClutter* self)
{
    GtPlayerBackendGstreamerClutterPrivate* priv = gt_player_backend_gstreamer_clutter_get_instance_private(self);

    clutter_gst_player_set_playing(CLUTTER_GST_PLAYER(priv->player), TRUE);
}

static void
stop(GtPlayerBackendGstreamerClutter* self)
{
    GtPlayerBackendGstreamerClutterPrivate* priv = gt_player_backend_gstreamer_clutter_get_instance_private(self);

    clutter_gst_player_set_playing(CLUTTER_GST_PLAYER(priv->player), FALSE);
}

static GtkWidget*
get_widget(GtPlayerBackend* backend)
{
    GtPlayerBackendGstreamerClutter* self = GT_PLAYER_BACKEND_GSTREAMER_CLUTTER(backend);
    GtPlayerBackendGstreamerClutterPrivate* priv = gt_player_backend_gstreamer_clutter_get_instance_private(self);

    return priv->widget;
}

static void
finalise(GObject* obj)
{
    GtPlayerBackendGstreamerClutter* self = GT_PLAYER_BACKEND_GSTREAMER_CLUTTER(obj);
    GtPlayerBackendGstreamerClutterPrivate* priv = gt_player_backend_gstreamer_clutter_get_instance_private(self);

    g_message("{GtPlayerBackendGstreamerClutter} Finalise");

    G_OBJECT_CLASS(gt_player_backend_gstreamer_clutter_parent_class)->finalize(obj);

    g_clear_object(&priv->widget);
    g_clear_object(&priv->player);

    g_free(priv->uri);
}

static void
get_property(GObject* obj,
             guint prop,
             GValue* val,
             GParamSpec* pspec)
{
    GtPlayerBackendGstreamerClutter* self = GT_PLAYER_BACKEND_GSTREAMER_CLUTTER(obj);
    GtPlayerBackendGstreamerClutterPrivate* priv = gt_player_backend_gstreamer_clutter_get_instance_private(self);

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
        case PROP_BUFFER_FILL:
            g_value_set_double(val, priv->buffer_fill);
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
    GtPlayerBackendGstreamerClutter* self = GT_PLAYER_BACKEND_GSTREAMER_CLUTTER(obj);
    GtPlayerBackendGstreamerClutterPrivate* priv = gt_player_backend_gstreamer_clutter_get_instance_private(self);

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
        case PROP_BUFFER_FILL:
            priv->buffer_fill = g_value_get_double(val);
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
gt_player_backend_gstreamer_clutter_class_finalize(GtPlayerBackendGstreamerClutterClass* klass)
{
}

static void
gt_player_backend_gstreamer_clutter_class_init(GtPlayerBackendGstreamerClutterClass* klass)
{
    GObjectClass* obj_class = G_OBJECT_CLASS(klass);
    static gboolean init = FALSE;

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
    props[PROP_BUFFER_FILL] = g_param_spec_double("buffer-fill",
                                                     "Buffer fill",
                                                     "Current buffer fill",
                                                     0, 1.0, 0,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    g_object_class_override_property(obj_class, PROP_VOLUME, "volume");
    g_object_class_override_property(obj_class, PROP_MUTED, "muted");
    g_object_class_override_property(obj_class, PROP_PLAYING, "playing");
    g_object_class_override_property(obj_class, PROP_URI, "uri");
    g_object_class_override_property(obj_class, PROP_BUFFER_FILL, "buffer-fill");

    if (!init)
    {
        gint res = gtk_clutter_init(NULL, NULL);
        clutter_gst_init(NULL, NULL);
        init = TRUE;
    }
}

static void
gt_player_backend_gstreamer_clutter_init(GtPlayerBackendGstreamerClutter* self)
{
    GtPlayerBackendGstreamerClutterPrivate* priv = gt_player_backend_gstreamer_clutter_get_instance_private(self);
    static const ClutterColor bg_colour = {0x00, 0x00, 0x00, 0x00};

    g_message("{GtPlayerBackendGstreamerClutter} Init");

    priv->widget = gtk_clutter_embed_new();
    priv->stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(priv->widget));
    priv->player = clutter_gst_playback_new();
    priv->video_actor = clutter_actor_new();
    priv->content = clutter_gst_aspectratio_new();

    g_object_ref(priv->widget);

    g_object_set(priv->content, "player", priv->player, NULL);
    g_object_set(priv->video_actor, "content", priv->content, NULL);

    clutter_actor_add_child(priv->stage, priv->video_actor);

    clutter_actor_set_background_color(priv->stage, &bg_colour);

    /* clutter_gst_playback_set_buffering_mode(priv->player, CLUTTER_GST_BUFFERING_MODE_STREAM); // In-memory buffering (let user choose?) */
    clutter_gst_playback_set_buffering_mode(priv->player, CLUTTER_GST_BUFFERING_MODE_DOWNLOAD); // On-disk buffering

    g_object_bind_property(self, "volume",
                           priv->player, "audio-volume",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(self, "uri",
                           priv->player, "uri",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(priv->player, "buffer-fill",
                           self, "buffer-fill",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

    g_signal_connect(priv->widget, "size-allocate", G_CALLBACK(size_allocate_cb), self); //TODO: Change this into a configure signal

}

G_MODULE_EXPORT
void
peas_register_types(PeasObjectModule* module)
{
    gt_player_backend_gstreamer_clutter_register_type(G_TYPE_MODULE(module));

    peas_object_module_register_extension_type(module,
                                               GT_TYPE_PLAYER_BACKEND,
                                               GT_TYPE_PLAYER_BACKEND_GSTREAMER_CLUTTER);
}
