#include "gt-player-clutter.h"
#include "gt-player.h"
#include "gt-app.h"

static const ClutterColor bg_colour = {0x00, 0x00, 0x00, 0x00};

typedef struct
{
    ClutterGstPlayback* player;
    ClutterActor* actor;
    ClutterContent* content;

    gdouble volume;
    GtChannel* open_channel;
} GtPlayerClutterPrivate;

static void gt_player_init(GtPlayerInterface* player);

G_DEFINE_TYPE_WITH_CODE(GtPlayerClutter, gt_player_clutter, GTK_CLUTTER_TYPE_EMBED,
                        G_ADD_PRIVATE(GtPlayerClutter)
                        G_IMPLEMENT_INTERFACE(GT_TYPE_PLAYER, gt_player_init));

enum 
{
    PROP_0,
    PROP_VOLUME,
    PROP_OPEN_CHANNEL,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtPlayerClutter*
gt_player_clutter_new(void)
{
    return g_object_new(GT_TYPE_PLAYER_CLUTTER, 
                        NULL);
}

static void
size_allocate_cb(GtkWidget* widget,
                 GdkRectangle* rect,
                 gpointer udata)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(widget);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    g_object_set(priv->actor,
                 "height", (gfloat) rect->height,
                 "width", (gfloat) rect->width,
                 NULL);
}

static void
finalize(GObject* object)
{
    GtPlayerClutter* self = (GtPlayerClutter*) object;
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    G_OBJECT_CLASS(gt_player_clutter_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(obj);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    switch (prop)
    {
        case PROP_VOLUME:
            g_value_set_double(val, priv->volume);
            break;
        case PROP_OPEN_CHANNEL:
            g_value_set_object(val, priv->open_channel);
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
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(obj);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    switch (prop)
    {
        case PROP_VOLUME:
            priv->volume = g_value_get_double(val);
            break;
        case PROP_OPEN_CHANNEL:
            g_clear_object(&priv->open_channel);
            priv->open_channel = g_value_dup_object(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
set_uri(GtPlayer* player, const gchar* uri)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(player);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    clutter_gst_playback_set_uri(priv->player, uri);
}

static void
play(GtPlayer* player)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(player); 
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    clutter_gst_player_set_playing(CLUTTER_GST_PLAYER(priv->player), TRUE);
}

static void
stop(GtPlayer* player)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(player); 
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    clutter_gst_player_set_playing(CLUTTER_GST_PLAYER(priv->player), FALSE);
}

static void
gt_player_init(GtPlayerInterface* iface)
{
    iface->set_uri = set_uri;
    iface->play = play;
    iface->stop = stop;
}

static void
gt_player_clutter_class_init(GtPlayerClutterClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_VOLUME] = g_param_spec_double("volume",
                                             "Volume",
                                             "Volume of player",
                                             0.0, 1.0, 0.3,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT);  
    props[PROP_OPEN_CHANNEL] = g_param_spec_object("open-channel",
                                                   "Open Channel",
                                                   "Currently open channel",
                                                   GT_TYPE_CHANNEL,
                                                   G_PARAM_READWRITE);

    g_object_class_override_property(object_class, PROP_VOLUME, "volume");
    g_object_class_override_property(object_class, PROP_OPEN_CHANNEL, "open-channel");
}

static void
gt_player_clutter_init(GtPlayerClutter* self)
{
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    priv->player = clutter_gst_playback_new();
    priv->actor = clutter_actor_new();
    priv->content = clutter_gst_aspectratio_new();

    g_object_set(priv->content, "player", priv->player, NULL);
    g_object_set(priv->actor, "content", priv->content, NULL);

    clutter_actor_add_child(gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(self)), priv->actor);
    clutter_actor_set_background_color(gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(self)), 
                                       &bg_colour);
    /* clutter_gst_playback_set_buffering_mode(priv->player, CLUTTER_GST_BUFFERING_MODE_STREAM); // In-memory buffering (let user choose?) */
    clutter_gst_playback_set_buffering_mode(priv->player, CLUTTER_GST_BUFFERING_MODE_DOWNLOAD); // On-disk buffering

    g_signal_connect(self, "size-allocate", G_CALLBACK(size_allocate_cb), NULL);

    g_object_bind_property(self, "volume",
                           priv->player, "audio-volume",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

    g_settings_bind(main_app->settings, "volume",
                    self, "volume",
                    G_SETTINGS_BIND_DEFAULT);
}

