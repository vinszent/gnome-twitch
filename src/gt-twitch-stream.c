#include "gt-twitch-stream.h"
#include "utils.h"

typedef struct
{
    gint64 id;

    gchar* status;
    gchar* game;
    gchar* name;
    gchar* display_name;

    GdkPixbuf* preview;

    gint64 viewers;
} GtTwitchStreamPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtTwitchStream, gt_twitch_stream, G_TYPE_OBJECT)

enum 
{
    PROP_0,
    PROP_ID,
    PROP_STATUS,
    PROP_GAME,
    PROP_NAME,
    PROP_DISPLAY_NAME,
    PROP_PREVIEW,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtTwitchStream*
gt_twitch_stream_new(gint64 id)
{
    return g_object_new(GT_TYPE_TWITCH_STREAM, 
                        "id", id,
                        NULL);
}

static void
finalize(GObject* object)
{
    GtTwitchStream* self = (GtTwitchStream*) object;
    GtTwitchStreamPrivate* priv = gt_twitch_stream_get_instance_private(self);

    g_free(priv->name);
    g_free(priv->display_name);
    g_free(priv->game);
    g_free(priv->status);

    g_clear_object(&priv->preview);

    G_OBJECT_CLASS(gt_twitch_stream_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtTwitchStream* self = GT_TWITCH_STREAM(obj);
    GtTwitchStreamPrivate* priv = gt_twitch_stream_get_instance_private(self);

    switch (prop)
    {
        case PROP_ID:
            g_value_set_int64(val, priv->id);
            break;
        case PROP_STATUS:
            g_value_set_string(val, priv->status);
            break;
        case PROP_NAME:
            g_value_set_string(val, priv->name);
            break;
        case PROP_DISPLAY_NAME:
            g_value_set_string(val, priv->display_name);
            break;
        case PROP_GAME:
            g_value_set_string(val, priv->game);
            break;
        case PROP_PREVIEW:
            g_value_set_object(val, priv->preview);
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
    GtTwitchStream* self = GT_TWITCH_STREAM(obj);
    GtTwitchStreamPrivate* priv = gt_twitch_stream_get_instance_private(self);

    switch (prop)
    {
        case PROP_ID:
            priv->id = g_value_get_int64(val);
            break;
        case PROP_STATUS:
            if (priv->status)
                g_free(priv->status);
            priv->status = g_value_dup_string(val);
            break;
        case PROP_NAME:
            if (priv->name)
                g_free(priv->name);
            priv->name = g_value_dup_string(val);
            break;
        case PROP_DISPLAY_NAME:
            if (priv->display_name)
                g_free(priv->display_name);
            priv->display_name = g_value_dup_string(val);
            break;
        case PROP_GAME:
            if (priv->game)
                g_free(priv->game);
                priv->game = g_value_dup_string_allow_null(val);
            break;
        case PROP_PREVIEW:
            if (priv->preview)
                g_object_unref(priv->preview);
            priv->preview = g_value_ref_sink_object(val);
            utils_pixbuf_scale_simple(&priv->preview,
                                      320, 180,
                                      GDK_INTERP_BILINEAR);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_twitch_stream_class_init(GtTwitchStreamClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_ID] = g_param_spec_int64("id",
                                       "ID",
                                       "ID of stream",
                                       0,
                                       G_MAXINT64,
                                       0,
                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

    props[PROP_NAME] = g_param_spec_string("name",
                                           "Name",
                                           "Name of stream",
                                           NULL,
                                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_STATUS] = g_param_spec_string("status",
                                           "Status",
                                           "Status of stream",
                                           NULL,
                                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_DISPLAY_NAME] = g_param_spec_string("display-name",
                                                   "Display Name",
                                                   "Display Name of stream",
                                                   NULL,
                                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_GAME] = g_param_spec_string("game",
                                           "Game",
                                           "Game being played by stream",
                                           NULL,
                                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_PREVIEW] = g_param_spec_object("preview",
                                              "Preview",
                                              "Preview of stream",
                                              GDK_TYPE_PIXBUF,
                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    g_object_class_install_properties(object_class,
                                      NUM_PROPS,
                                      props);
}

static void
gt_twitch_stream_init(GtTwitchStream* self)
{
}

void
gt_twitch_stream_free_list(GList* list)
{
    g_list_free_full(list, g_object_unref);
}
