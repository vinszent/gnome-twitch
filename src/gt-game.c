#include "gt-game.h"
#include "utils.h"
#include <glib/gi18n.h>

typedef struct
{
    gint id;

    gchar* name;

    GdkPixbuf* preview;

    GdkPixbuf* logo;

    gint64 viewers;
    gint64 channels;
} GtGamePrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtGame, gt_game, G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_ID,
    PROP_NAME,
    PROP_PREVIEW,
    PROP_LOGO,
    PROP_VIEWERS,
    PROP_CHANNELS,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtGame*
gt_game_new(const gchar* name, gint64 id)
{
    return g_object_new(GT_TYPE_GAME,
                        "name", name,
                        "id", id,
                        NULL);
}

static void
finalize(GObject* object)
{
    GtGame* self = (GtGame*) object;
    GtGamePrivate* priv = gt_game_get_instance_private(self);

    g_free(priv->name);

    g_clear_object(&priv->preview);

    g_clear_object(&priv->logo);

    G_OBJECT_CLASS(gt_game_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtGame* self = GT_GAME(obj);
    GtGamePrivate* priv = gt_game_get_instance_private(self);

    switch (prop)
    {
        case PROP_ID:
            g_value_set_int64(val, priv->id);
            break;
        case PROP_NAME:
            g_value_set_string(val, priv->name);
            break;
        case PROP_PREVIEW:
            g_value_set_object(val, priv->preview);
            break;
        case PROP_LOGO:
            g_value_set_object(val, priv->logo);
            break;
        case PROP_VIEWERS:
            g_value_set_int64(val, priv->viewers);
            break;
        case PROP_CHANNELS:
            g_value_set_int64(val, priv->channels);
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
    GtGame* self = GT_GAME(obj);
    GtGamePrivate* priv = gt_game_get_instance_private(self);

    switch (prop)
    {
        case PROP_ID:
            priv->id = g_value_get_int64(val);
            break;
        case PROP_NAME:
            g_free(priv->name);
            priv->name = g_value_dup_string(val);
            if (!priv->name)
                priv->name = _("Untitled broadcast");
            break;
        case PROP_PREVIEW:
            g_clear_object(&priv->preview);
            priv->preview = g_value_dup_object(val);
            utils_pixbuf_scale_simple(&priv->preview,
                                      200, 270,
                                      GDK_INTERP_BILINEAR);
            break;
        case PROP_LOGO:
            g_clear_object(&priv->logo);
            priv->logo = g_value_dup_object(val);
            break;
        case PROP_VIEWERS:
            priv->viewers = g_value_get_int64(val);
            break;
        case PROP_CHANNELS:
            priv->channels = g_value_get_int64(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_game_class_init(GtGameClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_ID] = g_param_spec_int64("id",
                                        "Id",
                                        "ID of game",
                                        0, G_MAXINT64, 0,
                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
    props[PROP_NAME] = g_param_spec_string("name",
                                           "Name",
                                           "Name of game",
                                           NULL,
                                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
    props[PROP_PREVIEW] = g_param_spec_object("preview",
                                              "Preview",
                                              "Preview of game",
                                              GDK_TYPE_PIXBUF,
                                              G_PARAM_READWRITE);
    props[PROP_LOGO] = g_param_spec_object("logo",
                                           "Logo",
                                           "Logo of game",
                                           GDK_TYPE_PIXBUF,
                                           G_PARAM_READWRITE);
    props[PROP_VIEWERS] = g_param_spec_int64("viewers",
                                             "Viewers",
                                             "Total viewers of game",
                                             0, G_MAXINT64, 0,
                                             G_PARAM_READWRITE);
    props[PROP_CHANNELS] = g_param_spec_int64("channels",
                                              "Channels",
                                              "Total channels of game",
                                              0, G_MAXINT64, 0,
                                              G_PARAM_READWRITE);

    g_object_class_install_properties(object_class,
                                      NUM_PROPS,
                                      props);
}

static void
gt_game_init(GtGame* self)
{
}

void
gt_game_update_from_raw_data(GtGame* self, GtGameRawData* data)
{
    g_object_set(self,
                 "preview", data->preview,
                 NULL);
}

void
gt_game_free_list(GList* list)
{
    g_list_free_full(list, g_object_unref);
}
