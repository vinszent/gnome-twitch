#include "gt-game.h"
#include "gt-app.h"
#include "utils.h"
#include <glib/gi18n.h>

typedef struct
{
    gint64 id;

    gchar* name;
    gchar* preview_url;

    gboolean updating;

    gchar* preview_filename;
    gint64 preview_timestamp;

    GdkPixbuf* preview;

    GdkPixbuf* logo;

    gint64 viewers;
    gint64 channels;

    GCancellable* cancel;
} GtGamePrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtGame, gt_game, G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_ID,
    PROP_NAME,
    PROP_UPDATING,
    PROP_PREVIEW_URL,
    PROP_PREVIEW,
    PROP_LOGO,
    PROP_VIEWERS,
    PROP_CHANNELS,
    NUM_PROPS
};

static GThreadPool* update_pool;

static GParamSpec* props[NUM_PROPS];

static inline void
set_preview(GtGame* self, GdkPixbuf* pic, gboolean save)
{
    GtGamePrivate* priv = gt_game_get_instance_private(self);

    g_clear_object(&priv->preview);
    priv->preview = pic;

    if (save)
        gdk_pixbuf_save(priv->preview, priv->preview_filename,
                        "jpeg", NULL, NULL);

    utils_pixbuf_scale_simple(&priv->preview,
                              200, 270,
                              GDK_INTERP_BILINEAR);

    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_PREVIEW]);
}

static void
update_func(gpointer data, gpointer udata)
{
    GtGame* self = GT_GAME(data);
    GtGamePrivate* priv = gt_game_get_instance_private(self);

    GdkPixbuf* pic = gt_twitch_download_picture(main_app->twitch,
                                                priv->preview_url,
                                                priv->preview_timestamp);

    if (pic)
    {
        set_preview(self, pic, TRUE);
        g_info("{GtGame} Updated preview for game '%s'", priv->name);
    }
}

static void
download_preview_cb(GObject* source,
                    GAsyncResult* res,
                    gpointer udata)
{
    GtGame* self = GT_GAME(udata);
    GtGamePrivate* priv = gt_game_get_instance_private(self);
    GError* error = NULL;

    GdkPixbuf* pic = g_task_propagate_pointer(G_TASK(res), &error);

    if (error)
    {
        g_error_free(error);
        return;
    }

    set_preview(self, pic, TRUE);

    priv->updating = FALSE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_UPDATING]);
}

static void
update_preview(GtGame* self)
{
    GtGamePrivate* priv = gt_game_get_instance_private(self);

    if (g_file_test(priv->preview_filename, G_FILE_TEST_EXISTS))
    {
        priv->preview_timestamp = utils_timestamp_file(priv->preview_filename);
        set_preview(self, gdk_pixbuf_new_from_file(priv->preview_filename, NULL), FALSE);
    }

    if (!priv->preview)
        gt_twitch_download_picture_async(main_app->twitch, priv->preview_url, 0,
                                         priv->cancel, download_preview_cb, self);
    else
    {
        g_thread_pool_push(update_pool, self, NULL);
    }
}

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
    g_free(priv->preview_url);
    g_free(priv->preview_filename);
    g_clear_object(&priv->preview);
    g_clear_object(&priv->logo);

    G_OBJECT_CLASS(gt_game_parent_class)->finalize(object);
}

static void
constructed(GObject* obj)
{
    GtGame* self = GT_GAME(obj);
    GtGamePrivate* priv = gt_game_get_instance_private(self);
    gchar* id = g_strdup_printf("%ld", priv->id);

    priv->preview_filename = g_build_filename(g_get_user_cache_dir(), "gnome-twitch", "games", id, NULL);

    g_free(id);

    G_OBJECT_CLASS(gt_game_parent_class)->constructed(obj);
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
        case PROP_UPDATING:
            g_value_set_boolean(val, priv->updating);
            break;
        case PROP_PREVIEW_URL:
            g_value_set_string(val, priv->preview_url);
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
        case PROP_PREVIEW_URL:
            g_free(priv->preview_url);
            priv->preview_url = g_value_dup_string(val);
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
    object_class->constructed = constructed;
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
    props[PROP_UPDATING] = g_param_spec_boolean("updating",
                                                "Updating",
                                                "Whether updating",
                                                FALSE,
                                                G_PARAM_READABLE);
    props[PROP_PREVIEW_URL] = g_param_spec_string("preview-url",
                                                  "Preview Url",
                                                  "Current preview url",
                                                  NULL,
                                                  G_PARAM_READWRITE);
    props[PROP_PREVIEW] = g_param_spec_object("preview",
                                              "Preview",
                                              "Preview of game",
                                              GDK_TYPE_PIXBUF,
                                              G_PARAM_READABLE);
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

    update_pool = g_thread_pool_new(update_func, NULL, 1, FALSE, NULL);
}

static void
gt_game_init(GtGame* self)
{
    GtGamePrivate* priv = gt_game_get_instance_private(self);

    priv->updating = FALSE;
    priv->preview = NULL;
}

void
gt_game_update_from_raw_data(GtGame* self, GtGameRawData* data)
{
    GtGamePrivate* priv = gt_game_get_instance_private(self);

    priv->updating = TRUE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_UPDATING]);

    g_object_set(self,
                 "preview-url", data->preview_url,
                 NULL);

    update_preview(self);
}

void
gt_game_free_list(GList* list)
{
    g_list_free_full(list, g_object_unref);
}
