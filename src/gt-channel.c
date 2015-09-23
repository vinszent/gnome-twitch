#include "gt-channel.h"
#include "gt-app.h"
#include "utils.h"
#include <json-glib/json-glib.h>

#define N_JSON_PROPS 2

typedef struct
{
    gint64 id;

    gchar* status;
    gchar* game;
    gchar* name;
    gchar* display_name;

    GdkPixbuf* preview;
    GdkPixbuf* video_banner;

    gint64 viewers;
    GDateTime* stream_started_time; 

    gboolean favourited;
    gboolean online;
    gboolean auto_update;

    guint update_id;
} GtChannelPrivate;

static GThreadPool* update_pool;

static void
json_serializable_iface_init(JsonSerializableIface* iface);

G_DEFINE_TYPE_WITH_CODE(GtChannel, gt_channel, G_TYPE_OBJECT,
                        G_ADD_PRIVATE(GtChannel)
                        G_IMPLEMENT_INTERFACE(JSON_TYPE_SERIALIZABLE, json_serializable_iface_init))

enum 
{
    PROP_0,
    PROP_ID,
    PROP_STATUS,
    PROP_GAME,
    PROP_NAME,
    PROP_DISPLAY_NAME,
    PROP_PREVIEW,
    PROP_VIDEO_BANNER,
    PROP_VIEWERS,
    PROP_STREAM_STARTED_TIME,
    PROP_FAVOURITED,
    PROP_ONLINE,
    PROP_AUTO_UPDATE,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtChannel*
gt_channel_new(const gchar* name, gint64 id)
{
    return g_object_new(GT_TYPE_CHANNEL, 
                        "name", name,
                        "id", id,
                        NULL);
}

typedef struct
{
    GtChannel* self;
    GtChannelRawData* raw;
} UpdateSetData;

static gboolean
update_set_cb(gpointer udata)
{
    UpdateSetData* setd = (UpdateSetData*) udata;

    gt_channel_update_from_raw_data(setd->self, setd->raw);

    gt_twitch_channel_raw_data_free(setd->raw);
    g_free(setd);

    return FALSE;
}

static void
update_cb(gpointer data,
          gpointer udata)
{
    GtChannel* self = GT_CHANNEL(data);
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    GtChannelRawData* raw = gt_twitch_channel_with_stream_raw_data(main_app->twitch, priv->name);
    UpdateSetData* setd = g_malloc(sizeof(UpdateSetData));
    setd->self = self;
    setd->raw = raw;

    g_idle_add((GSourceFunc) update_set_cb, setd);
}

static gboolean
update(GtChannel* self)
{
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    g_thread_pool_push(update_pool, self, NULL);

    return TRUE;
}

static void
auto_update_cb(GObject* src,
               GParamSpec* pspec,
               gpointer udata)
{
    GtChannel* self = GT_CHANNEL(src);
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    if (priv->auto_update)
        priv->update_id = g_timeout_add(5e3, (GSourceFunc) update, self); //TODO: Add this as a setting
    else
        g_source_remove(priv->update_id);
}

static void
finalize(GObject* object)
{
    GtChannel* self = (GtChannel*) object;
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    g_free(priv->name);
    g_free(priv->display_name);
    g_free(priv->game);
    g_free(priv->status);

    g_date_time_unref(priv->stream_started_time);

    g_clear_object(&priv->preview);
    g_clear_object(&priv->video_banner);

    if (priv->update_id > 0)
        g_source_remove(priv->update_id);

    G_OBJECT_CLASS(gt_channel_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtChannel* self = GT_CHANNEL(obj);
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

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
        case PROP_VIDEO_BANNER:
            g_value_set_object(val, priv->video_banner);
            break;
        case PROP_VIEWERS:
            g_value_set_int64(val, priv->viewers);
            break;
        case PROP_STREAM_STARTED_TIME:
            g_date_time_ref(priv->stream_started_time);
            g_value_set_pointer(val, priv->stream_started_time);
            break;    
        case PROP_FAVOURITED:
            g_value_set_boolean(val, priv->favourited);
            break;
        case PROP_ONLINE:
            g_value_set_boolean(val, priv->online);
            break;
        case PROP_AUTO_UPDATE:
            g_value_set_boolean(val, priv->auto_update);
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
    GtChannel* self = GT_CHANNEL(obj);
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

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
        case PROP_VIDEO_BANNER:
            if (priv->video_banner)
                g_object_unref(priv->video_banner);
            priv->video_banner = g_value_ref_sink_object(val);
            utils_pixbuf_scale_simple(&priv->video_banner,
                                      320, 180,
                                      GDK_INTERP_BILINEAR);
            break;
        case PROP_VIEWERS:
            priv->viewers = g_value_get_int64(val);
            break;
        case PROP_STREAM_STARTED_TIME:
            if (priv->stream_started_time)
                g_date_time_unref(priv->stream_started_time);
            priv->stream_started_time = g_value_get_pointer(val);
            if (priv->stream_started_time)
                g_date_time_ref(priv->stream_started_time);
            break;
        case PROP_FAVOURITED:
            priv->favourited = g_value_get_boolean(val);
            break;
        case PROP_ONLINE:
            priv->online = g_value_get_boolean(val);
            break;
        case PROP_AUTO_UPDATE:
            priv->auto_update = g_value_get_boolean(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_channel_class_init(GtChannelClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_ID] = g_param_spec_int64("id",
                                       "ID",
                                       "ID of channel",
                                       0,
                                       G_MAXINT64,
                                       0,
                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

    props[PROP_NAME] = g_param_spec_string("name",
                                           "Name",
                                           "Name of channel",
                                           NULL,
                                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_STATUS] = g_param_spec_string("status",
                                           "Status",
                                           "Status of channel",
                                           NULL,
                                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_DISPLAY_NAME] = g_param_spec_string("display-name",
                                                   "Display Name",
                                                   "Display Name of channel",
                                                   NULL,
                                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_GAME] = g_param_spec_string("game",
                                           "Game",
                                           "Game being played by channel",
                                           NULL,
                                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_PREVIEW] = g_param_spec_object("preview",
                                              "Preview",
                                              "Preview of channel",
                                              GDK_TYPE_PIXBUF,
                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_VIDEO_BANNER] = g_param_spec_object("video-banner",
                                                   "Video Banner",
                                                   "Video banner of channel",
                                                   GDK_TYPE_PIXBUF,
                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_VIEWERS] = g_param_spec_int64("viewers",
                                             "Viewers",
                                             "Number of viewers",
                                             0, G_MAXINT64, 0,
                                             G_PARAM_READWRITE);
    props[PROP_STREAM_STARTED_TIME] = g_param_spec_pointer("stream-started-time",
                                                  "Stream started time",
                                                  "Stream started time",
                                                  G_PARAM_READWRITE);
    props[PROP_FAVOURITED] = g_param_spec_boolean("favourited",
                                                  "Favourited",
                                                  "Whether the channel is favourited",
                                                  FALSE,
                                                  G_PARAM_READWRITE);
    props[PROP_ONLINE] = g_param_spec_boolean("online",
                                              "Online",
                                              "Whether the channel is online",
                                              FALSE,
                                              G_PARAM_READWRITE);
    props[PROP_AUTO_UPDATE] = g_param_spec_boolean("auto-update",
                                                   "Auto Update",
                                                   "Whether it should update itself automatically",
                                                   FALSE,
                                                   G_PARAM_READWRITE);

    g_object_class_install_properties(object_class,
                                      NUM_PROPS,
                                      props);

    update_pool = g_thread_pool_new((GFunc) update_cb, NULL, 2, FALSE, NULL);
}

static void
gt_channel_init(GtChannel* self)
{
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    g_signal_connect(self, "notify::auto-update", G_CALLBACK(auto_update_cb), NULL);

    priv->favourited = gt_favourites_manager_is_channel_favourited(main_app->fav_mgr, self);
    
    gt_favourites_manager_attach_to_channel(main_app->fav_mgr, self);
}

static GParamSpec**
json_list_props(JsonSerializable* json,
                guint* n_pspecs)
{
    GParamSpec** json_props = g_malloc_n(N_JSON_PROPS, sizeof(GParamSpec*));

    json_props[0] = props[PROP_NAME];
    json_props[1] = props[PROP_ID];

    *n_pspecs = N_JSON_PROPS;

    return json_props;
}

static void
json_serializable_iface_init(JsonSerializableIface* iface)
{
    iface->list_properties = json_list_props;
}

void
gt_channel_update_from_raw_data(GtChannel* self, GtChannelRawData* data)
{
    g_object_set(self,
                 "viewers", data->viewers,
                 "display-name", data->display_name,
                 "status", data->status,
                 "game", data->game,
                 "preview", data->preview,
                 "video-banner", data->video_banner,
                 "stream-started-time", data->stream_started_time,
                 "online", data->online,
                 NULL);
}

void
gt_channel_toggle_favourited(GtChannel* self)
{
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    priv->favourited = !priv->favourited;

    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_FAVOURITED]);
}

void
gt_channel_free_list(GList* list)
{
    g_list_free_full(list, g_object_unref);
}

gboolean
gt_channel_compare(GtChannel* self,
                          gpointer other)
{
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);
    gboolean ret = TRUE;

    if (GT_IS_CHANNEL(other))
    {
        gchar* name;
        gint64 id;

        g_object_get(G_OBJECT(other),
                     "name", &name,
                     "id", &id,
                     NULL);


        ret = !(g_strcmp0(priv->name, name) == 0 && priv->id == id);

        g_free(name);
    }

    return ret;
}
