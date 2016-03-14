#include "gt-favourites-manager.h"
#include "gt-app.h"
#include <json-glib/json-glib.h>

#define FAV_CHANNELS_FILE g_build_filename(g_get_user_data_dir(), "gnome-twitch", "favourite-channels.json", NULL);

typedef struct
{
    guint auto_update_id;
    guint auto_update_interval;
    void* tmp;
} GtFavouritesManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtFavouritesManager, gt_favourites_manager, G_TYPE_OBJECT)

enum 
{
    PROP_0,
    NUM_PROPS
};

enum
{
    SIG_CHANNEL_FAVOURITED,
    SIG_CHANNEL_UNFAVOURITED,
    NUM_SIGS
};

static GParamSpec* props[NUM_PROPS];

static guint sigs[NUM_SIGS];

GtFavouritesManager*
gt_favourites_manager_new(void)
{
    return g_object_new(GT_TYPE_FAVOURITES_MANAGER, 
                        NULL);
}

static void
channel_online_cb(GObject* source,
                  GParamSpec* pspe,
                  gpointer udata)
{
    GtFavouritesManager* self = GT_FAVOURITES_MANAGER(udata);
    GtFavouritesManagerPrivate* priv = gt_favourites_manager_get_instance_private(self);
    gboolean online;
    gchar* name;
    gchar* game;
    gchar* title;
    gchar* body;

    g_object_get(source, 
                 "online", &online, 
                 "name", &name,
                 "game", &game,
                 NULL);

    if (online)
    {   
        GNotification* notification;

        title = g_strdup_printf("Channel %s is now online", name);
        body = g_strdup_printf("Streaming %s", game);

        notification = g_notification_new(title);
        g_notification_set_body(notification, body);
        g_application_send_notification(G_APPLICATION(main_app), NULL, notification);

        g_object_unref(notification);
    }

    g_free(name);
    g_free(game);
}

static void
channel_favourited_cb(GObject* source,
                      GParamSpec* pspec,
                      gpointer udata)
{
    GtFavouritesManager* self = GT_FAVOURITES_MANAGER(udata);
    GtFavouritesManagerPrivate* priv = gt_favourites_manager_get_instance_private(self);
    GtChannel* chan = GT_CHANNEL(source);

    gboolean favourited;

    g_object_get(chan, "favourited", &favourited, NULL);
    
    if (favourited)
    {
        self->favourite_channels = g_list_append(self->favourite_channels, chan);
        g_signal_connect(chan, "notify::online", G_CALLBACK(channel_online_cb), self);
        g_object_ref(chan);

        g_message("{GtChannel} Favourited '%s' (%p)", gt_channel_get_name(chan), chan);

        g_signal_emit(self, sigs[SIG_CHANNEL_FAVOURITED], 0, chan);
    }
    else
    {
        GList* found = g_list_find_custom(self->favourite_channels, chan, (GCompareFunc) gt_channel_compare);
        // Should never return null;

        g_signal_handlers_disconnect_by_func(found->data, channel_online_cb, self);

        g_message("{GtChannel} Unfavourited '%s' (%p)", gt_channel_get_name(chan), chan);

        g_signal_emit(self, sigs[SIG_CHANNEL_UNFAVOURITED], 0, found->data);

        g_clear_object(&found->data);
        self->favourite_channels = g_list_delete_link(self->favourite_channels, found);
    }
}

static void
oneshot_updating_cb(GObject* source,
                    GParamSpec* pspec,
                    gpointer udata)
{
    GtFavouritesManager* self = GT_FAVOURITES_MANAGER(udata);
    gboolean updating;

    g_object_get(source, "updating", &updating, NULL);

    if (!updating)
    {
        g_signal_connect(source, "notify::online", G_CALLBACK(channel_online_cb), self);
        g_signal_handlers_disconnect_by_func(source, oneshot_updating_cb, self); // Just run once after first update
    }
}

static gboolean
auto_update_cb(GtFavouritesManager* self)
{
    for (GList* l = self->favourite_channels; l != NULL; l = l->next)
        gt_channel_update(GT_CHANNEL(l->data));

    return G_SOURCE_CONTINUE;
}

static void
auto_update_interval_changed_cb(GSettings* settings,
                           const gchar* key,
                           GtFavouritesManager* self)
{
    GtFavouritesManagerPrivate* priv = gt_favourites_manager_get_instance_private(self);

    priv->auto_update_interval = g_settings_get_uint(settings, key);
    gt_favourites_manager_update(self);
}

static void
shutdown_cb(GApplication* app,
            gpointer udata)
{
    GtFavouritesManager* self = GT_FAVOURITES_MANAGER(udata);

    gt_favourites_manager_save(self);
}

static void
finalize(GObject* object)
{
    GtFavouritesManager* self = (GtFavouritesManager*) object;
    GtFavouritesManagerPrivate* priv = gt_favourites_manager_get_instance_private(self);

    if (priv->auto_update_id != 0)
        g_source_remove(priv->auto_update_id);

    G_OBJECT_CLASS(gt_favourites_manager_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtFavouritesManager* self = GT_FAVOURITES_MANAGER(obj);
    GtFavouritesManagerPrivate* priv = gt_favourites_manager_get_instance_private(self);

    switch (prop)
    {
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
    GtFavouritesManager* self = GT_FAVOURITES_MANAGER(obj);
    GtFavouritesManagerPrivate* priv = gt_favourites_manager_get_instance_private(self);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_favourites_manager_class_init(GtFavouritesManagerClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    sigs[SIG_CHANNEL_FAVOURITED] = g_signal_new("channel-favourited",
                                                GT_TYPE_FAVOURITES_MANAGER,
                                                G_SIGNAL_RUN_LAST,
                                                0, NULL, NULL,
                                                g_cclosure_marshal_VOID__OBJECT,
                                                G_TYPE_NONE,
                                                1, GT_TYPE_CHANNEL);
    sigs[SIG_CHANNEL_UNFAVOURITED] = g_signal_new("channel-unfavourited",
                                                GT_TYPE_FAVOURITES_MANAGER,
                                                G_SIGNAL_RUN_LAST,
                                                0, NULL, NULL,
                                                g_cclosure_marshal_VOID__OBJECT,
                                                G_TYPE_NONE,
                                                1, GT_TYPE_CHANNEL);
}

static void
gt_favourites_manager_init(GtFavouritesManager* self)
{
    GtFavouritesManagerPrivate* priv = gt_favourites_manager_get_instance_private(self);

    auto_update_interval_changed_cb(main_app->settings, "auto-update-interval", self);
    g_signal_connect(main_app->settings,
                     "changed::auto-update-interval",
                     G_CALLBACK(auto_update_interval_changed_cb),
                     self);

    g_signal_connect(main_app, "shutdown", G_CALLBACK(shutdown_cb), self);
}

void
gt_favourites_manager_load(GtFavouritesManager* self)
{
    GtFavouritesManagerPrivate* priv = gt_favourites_manager_get_instance_private(self);
    JsonParser* parse = json_parser_new();
    JsonNode* root;
    JsonArray* jarr;
    gchar* fp = FAV_CHANNELS_FILE;
    GError* err = NULL;

    if (!g_file_test(fp, G_FILE_TEST_EXISTS))
        goto finish;

    json_parser_load_from_file(parse, fp, &err);

    if (err)
    {
        g_warning("{GtFavouritesManager} Error loading favourite channels '%s'", err->message);
        goto finish;
    }

    root = json_parser_get_root(parse);
    jarr = json_node_get_array(root);

    for (GList* l = json_array_get_elements(jarr); l != NULL; l = l->next)
    {
        GtChannel* chan = GT_CHANNEL(json_gobject_deserialize(GT_TYPE_CHANNEL, l->data));
        self->favourite_channels = g_list_append(self->favourite_channels, chan);
        g_signal_handlers_block_by_func(chan, channel_favourited_cb, self);
        g_object_set(chan, "favourited", TRUE, NULL);
        g_signal_handlers_unblock_by_func(chan, channel_favourited_cb, self);

        g_signal_connect(chan, "notify::updating", G_CALLBACK(oneshot_updating_cb), self);
    }

    gt_favourites_manager_update(self);

finish:
    g_object_unref(parse);
    g_free(fp);
}

void
gt_favourites_manager_save(GtFavouritesManager* self)
{
    GtFavouritesManagerPrivate* priv = gt_favourites_manager_get_instance_private(self);
    JsonArray* jarr = json_array_new();
    JsonGenerator* gen = json_generator_new();
    JsonNode* final = json_node_new(JSON_NODE_ARRAY);
    gchar* fp = FAV_CHANNELS_FILE;

    for (GList* l = self->favourite_channels; l != NULL; l = l->next)
    {
        JsonNode* node = json_gobject_serialize(l->data);
        json_array_add_element(jarr, node);
    }

    final = json_node_init_array(final, jarr);
    json_array_unref(jarr);

    json_generator_set_root(gen, final);
    json_generator_to_file(gen, fp, NULL);

    json_node_free(final);
    g_object_unref(gen);
    g_free(fp);
}

gboolean
gt_favourites_manager_is_channel_favourited(GtFavouritesManager* self, GtChannel* chan)
{
    return g_list_find_custom(self->favourite_channels, chan, (GCompareFunc) gt_channel_compare) != NULL;
}

void
gt_favourites_manager_attach_to_channel(GtFavouritesManager* self, GtChannel* chan)
{
    g_signal_connect(chan, "notify::favourited", G_CALLBACK(channel_favourited_cb), self);
}

void
gt_favourites_manager_update(GtFavouritesManager* self)
{
    GtFavouritesManagerPrivate* priv = gt_favourites_manager_get_instance_private(self);

    auto_update_cb(self);
    if (priv->auto_update_id != 0)
        g_source_remove(priv->auto_update_id);
    priv->auto_update_id = g_timeout_add_seconds(priv->auto_update_interval,
                                                 (GSourceFunc) auto_update_cb,
                                                 self);
}
