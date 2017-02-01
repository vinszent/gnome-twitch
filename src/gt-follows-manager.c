#include "gt-follows-manager.h"
#include "gt-app.h"
#include "gt-win.h"
#include <json-glib/json-glib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#define TAG "GtFollowsManager"
#include "gnome-twitch/gt-log.h"

#define OLD_FAV_CHANNELS_FILE g_build_filename(g_get_user_data_dir(), "gnome-twitch", "favourite-channels.json", NULL);
#define FAV_CHANNELS_FILE g_build_filename(g_get_user_data_dir(), "gnome-twitch", "followed-channels.json", NULL);

typedef struct
{
    void* tmp;
} GtFollowsManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtFollowsManager, gt_follows_manager, G_TYPE_OBJECT)

enum
{
    PROP_0,
    NUM_PROPS
};

enum
{
    SIG_CHANNEL_FOLLOWED,
    SIG_CHANNEL_UNFOLLOWED,
    SIG_STARTED_LOADING_FOLLOWS,
    SIG_FINISHED_LOADING_FOLLOWS,
    NUM_SIGS
};

static GParamSpec* props[NUM_PROPS];

static guint sigs[NUM_SIGS];

GtFollowsManager*
gt_follows_manager_new(void)
{
    return g_object_new(GT_TYPE_FOLLOWS_MANAGER,
                        NULL);
}

static void
channel_online_cb(GObject* source,
                  GParamSpec* pspe,
                  gpointer udata)
{
    GtFollowsManager* self = GT_FOLLOWS_MANAGER(udata);
    GtFollowsManagerPrivate* priv = gt_follows_manager_get_instance_private(self);
    gboolean online;
    gchar* name;
    gchar* game;

    g_object_get(source,
                 "online", &online,
                 "name", &name,
                 "game", &game,
                 NULL);

    if (online)
    {
        GNotification* notification;
        gchar title_str[100];
        gchar body_str[100];

        g_sprintf(title_str, _("Channel %s is now online"), name);
        g_sprintf(body_str, _("Streaming %s"), game);

        notification = g_notification_new(title_str);
        g_notification_set_body(notification, body_str);
        g_application_send_notification(G_APPLICATION(main_app), NULL, notification);

        g_object_unref(notification);
    }
}

static void
logged_in_cb(GObject* source,
             GParamSpec* pspec,
             gpointer udata)
{
    GtFollowsManager* self = GT_FOLLOWS_MANAGER(udata);

    if (gt_app_credentials_valid(main_app))
        gt_follows_manager_load_from_twitch(self);
}

static void
channel_followed_cb(GObject* source,
                      GParamSpec* pspec,
                      gpointer udata);
static gboolean
toggle_followed_cb(gpointer udata)
{
    GtChannel* chan = GT_CHANNEL(udata);
    GtFollowsManager* self = main_app->fav_mgr;

    g_signal_handlers_block_by_func(chan, channel_followed_cb, self);
    gt_channel_toggle_followed(chan);
    g_signal_handlers_unblock_by_func(chan, channel_followed_cb, self);

    return G_SOURCE_REMOVE;
}

static void
channel_followed_cb(GObject* source,
                      GParamSpec* pspec,
                      gpointer udata)
{
    GtFollowsManager* self = GT_FOLLOWS_MANAGER(udata);
    GtFollowsManagerPrivate* priv = gt_follows_manager_get_instance_private(self);
    GtChannel* chan = GT_CHANNEL(source);

    gboolean followed;

    g_object_get(chan, "followed", &followed, NULL);

    if (followed)
    {
        if (gt_app_credentials_valid(main_app))
        {
            GError* error = NULL;

            gt_twitch_follow_channel(main_app->twitch, gt_channel_get_name(chan), &error); //TODO: Error handling
//            gt_twitch_follow_channel_async(main_app->twitch, gt_channel_get_name(chan), NULL, NULL); //TODO: Error handling

            if (error)
            {
                gchar* secondary = g_strdup_printf(_("Unable to follow channel '%s' on Twitch, "
                                                     "try refreshing your login"),
                                                   gt_channel_get_name(chan));

                gt_win_show_error_message(GT_WIN_ACTIVE, secondary, error->message);

                g_idle_add((GSourceFunc) toggle_followed_cb, chan);

                g_free(secondary);
                g_error_free(error);

                return;
            }
        }

        self->follow_channels = g_list_append(self->follow_channels, chan);
//        g_signal_connect(chan, "notify::online", G_CALLBACK(channel_online_cb), self);
        g_object_ref(chan);

        MESSAGEF("Followed channel '%s'", gt_channel_get_name(chan));

        g_signal_emit(self, sigs[SIG_CHANNEL_FOLLOWED], 0, chan);
    }
    else
    {
        GList* found = g_list_find_custom(self->follow_channels, chan, (GCompareFunc) gt_channel_compare);
        // Should never return null;

        g_assert_nonnull(found);

//        g_signal_handlers_disconnect_by_func(found->data, channel_online_cb, self);

        if (gt_app_credentials_valid(main_app))
        {
            GError* error = NULL;
//            gt_twitch_unfollow_channel_async(main_app->twitch, gt_channel_get_name(chan), NULL, NULL); //TODO: Error handling
            gt_twitch_unfollow_channel(main_app->twitch, gt_channel_get_name(chan), &error);

            if (error)
            {
                gchar* secondary = g_strdup_printf(_("Unable to unfollow channel '%s' on Twitch, "
                                                     "try refreshing your login"),
                                                   gt_channel_get_name(chan));

                gt_win_show_error_message(GT_WIN_ACTIVE, secondary, error->message);

                g_idle_add((GSourceFunc) toggle_followed_cb, chan);

                g_free(secondary);
                g_error_free(error);

                return;
            }
        }

        // Remove the link before the signal is emitted
        self->follow_channels = g_list_remove_link(self->follow_channels, found);

        MESSAGEF("Unfollowed channel '%s'", gt_channel_get_name(chan));

        g_signal_emit(self, sigs[SIG_CHANNEL_UNFOLLOWED], 0, found->data);

        // Unref here so that the GtChannel has a ref while the signal is being emitted
        g_clear_object(&found->data);
        g_list_free(found);
    }
}

static void
oneshot_updating_cb(GObject* source,
                    GParamSpec* pspec,
                    gpointer udata)
{
    GtFollowsManager* self = GT_FOLLOWS_MANAGER(udata);
    gboolean updating;

    g_object_get(source, "updating", &updating, NULL);

    if (!updating)
    {
//        g_signal_connect(source, "notify::online", G_CALLBACK(channel_online_cb), self);
        g_signal_handlers_disconnect_by_func(source, oneshot_updating_cb, self); // Just run once after first update
    }
}

static void
shutdown_cb(GApplication* app,
            gpointer udata)
{
    GtFollowsManager* self = GT_FOLLOWS_MANAGER(udata);

//    gt_follows_manager_save(self);
}

static void
finalize(GObject* object)
{
    GtFollowsManager* self = (GtFollowsManager*) object;
    GtFollowsManagerPrivate* priv = gt_follows_manager_get_instance_private(self);

    G_OBJECT_CLASS(gt_follows_manager_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtFollowsManager* self = GT_FOLLOWS_MANAGER(obj);
    GtFollowsManagerPrivate* priv = gt_follows_manager_get_instance_private(self);

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
    GtFollowsManager* self = GT_FOLLOWS_MANAGER(obj);
    GtFollowsManagerPrivate* priv = gt_follows_manager_get_instance_private(self);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_follows_manager_class_init(GtFollowsManagerClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    sigs[SIG_CHANNEL_FOLLOWED] = g_signal_new("channel-followed",
                                                GT_TYPE_FOLLOWS_MANAGER,
                                                G_SIGNAL_RUN_LAST,
                                                0, NULL, NULL,
                                                NULL,
                                                G_TYPE_NONE,
                                                1, GT_TYPE_CHANNEL);
    sigs[SIG_CHANNEL_UNFOLLOWED] = g_signal_new("channel-unfollowed",
                                                  GT_TYPE_FOLLOWS_MANAGER,
                                                  G_SIGNAL_RUN_LAST,
                                                  0, NULL, NULL,
                                                  NULL,
                                                  G_TYPE_NONE,
                                                  1, GT_TYPE_CHANNEL);
    sigs[SIG_STARTED_LOADING_FOLLOWS] = g_signal_new("started-loading-follows",
                                                        GT_TYPE_FOLLOWS_MANAGER,
                                                        G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                                                        0, NULL, NULL, NULL,
                                                        G_TYPE_NONE, 0, NULL);
    sigs[SIG_FINISHED_LOADING_FOLLOWS] = g_signal_new("finished-loading-follows",
                                                        GT_TYPE_FOLLOWS_MANAGER,
                                                        G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                                                        0, NULL, NULL, NULL,
                                                        G_TYPE_NONE, 0, NULL);
}

static void
gt_follows_manager_init(GtFollowsManager* self)
{
    gchar* old_fp = OLD_FAV_CHANNELS_FILE;
    gchar* new_fp = FAV_CHANNELS_FILE;

    g_signal_connect(main_app, "shutdown", G_CALLBACK(shutdown_cb), self);
//    g_signal_connect(main_app, "notify::oauth-token", G_CALLBACK(logged_in_cb), self);
    g_signal_connect(main_app, "notify::user-name", G_CALLBACK(logged_in_cb), self);


    //TODO: Remove this in a release or two
    if (g_file_test(old_fp, G_FILE_TEST_EXISTS))
        g_rename(old_fp, new_fp);

    g_free(old_fp);
    g_free(new_fp);
}

static void
follow_channel_cb(GObject* source,
                  GAsyncResult* res,
                  gpointer udata)
{
    GList* l = udata;
    GError* error = NULL;
    gboolean success = g_task_propagate_boolean(G_TASK(res), &error);

    if (error)
    {
        GtWin* self = g_list_last(l)->data;

        gt_win_show_error_message(GT_WIN_ACTIVE,
                                  "Unable to move your follows to Twitch, try refreshing your login",
                                  error->message);
        g_error_free(error);

        g_signal_emit(self, sigs[SIG_FINISHED_LOADING_FOLLOWS], 0);

        return;
    }

    if (GT_IS_CHANNEL(l->data))
    {
        gt_twitch_follow_channel_async(main_app->twitch,
                                       gt_channel_get_name(GT_CHANNEL(l->data)),
                                       follow_channel_cb, l->next);
    }
    else if (GT_IS_FOLLOWS_MANAGER(l->data))
    {
        gchar* fp = FAV_CHANNELS_FILE;
        gchar* new_fp = g_strconcat(fp, ".bak", NULL);

        g_rename(fp, new_fp);

        gt_follows_manager_load_from_twitch(GT_FOLLOWS_MANAGER(l->data));

        g_free(fp);
        g_free(new_fp);
    }
    else
        g_assert_not_reached();
}

static void
move_local_follows_cb(GtkInfoBar* bar,
                         gint res,
                         gpointer udata)
{
    GtFollowsManager* self = GT_FOLLOWS_MANAGER(udata);

    if (res == GTK_RESPONSE_YES)
    {
        gchar* fp = FAV_CHANNELS_FILE;
        JsonParser* parse = json_parser_new();
        JsonNode* root;
        JsonArray* jarr;
        GError* err = NULL;
        GList* l = NULL;
        GList* channels = NULL;

        json_parser_load_from_file(parse, fp, &err);

        if (err)
        {
            g_warning("{GtFollowsManager} Error moving local follow channels to twitch '%s'", err->message);
            return;
        }

        root = json_parser_get_root(parse);
        jarr = json_node_get_array(root);
        l = json_array_get_elements(jarr);

        g_assert(g_list_length(l) > 0);

        for (; l != NULL; l = l->next)
        {
            channels = g_list_append(channels,
                                     json_gobject_deserialize(GT_TYPE_CHANNEL, l->data));
        }

        channels = g_list_append(channels, self);
        gt_twitch_follow_channel_async(main_app->twitch, gt_channel_get_name(GT_CHANNEL(channels->data)),
                                       follow_channel_cb, channels->next);

        g_object_unref(parse);
        g_free(fp);
    }
    else
    {
        gchar* fp = FAV_CHANNELS_FILE;
        gchar* new_fp = g_strconcat(fp, ".bak", NULL);

        g_rename(fp, new_fp);

        gt_follows_manager_load_from_twitch(self);

        g_free(fp);
        g_free(new_fp);
    }
}

static void
follows_all_cb(GObject* source,
               GAsyncResult* res,
               gpointer udata)
{
    GtFollowsManager* self = GT_FOLLOWS_MANAGER(udata);
    GError* err = NULL;
    GList* list = NULL;
    gchar* fp = FAV_CHANNELS_FILE;

    list = gt_twitch_fetch_all_followed_channels_finish(GT_TWITCH(source), res, &err);

    g_clear_pointer(&self->follow_channels,
        (GDestroyNotify) gt_channel_list_free);

    if (err)
    {
        GtWin* win = GT_WIN_ACTIVE;

        g_assert(GT_IS_WIN(win));

        gt_win_show_error_message(GT_WIN_ACTIVE,
            "Unable to get your Twitch follows",
            err->message);

        g_error_free(err);

        return;
    }

    self->follow_channels = list;

    if (self->follow_channels)
    {
        for (GList* l = list; l != NULL; l = l->next)
        {
            GtChannel* chan = l->data;

            g_assert(GT_IS_CHANNEL(chan));
            g_assert(g_object_is_floating(chan));

            g_object_ref_sink(chan);

            g_signal_handlers_block_by_func(chan, channel_followed_cb, self);
            g_object_set(chan,
                         "auto-update", TRUE,
                         "followed", TRUE,
                         NULL);
            g_object_ref_sink(G_OBJECT(chan));
            g_signal_emit(self, sigs[SIG_CHANNEL_FOLLOWED], 0, chan);
            g_signal_handlers_unblock_by_func(chan, channel_followed_cb, self);
        }

    }

    if (g_file_test(fp, G_FILE_TEST_EXISTS))
    {
        gt_win_ask_question(GT_WIN_ACTIVE,
                            _("GNOME Twitch has detected local follows,"
                              " would you like to move them to Twitch?"),
                            G_CALLBACK(move_local_follows_cb), self);
    }
    else
        g_signal_emit(self, sigs[SIG_FINISHED_LOADING_FOLLOWS], 0);

    g_free(fp);
}

void
gt_follows_manager_load_from_twitch(GtFollowsManager* self)
{
    g_signal_emit(self, sigs[SIG_STARTED_LOADING_FOLLOWS], 0);

    const GtUserInfo* info = gt_app_get_user_info(main_app);

    gt_twitch_fetch_all_followed_channels_async(main_app->twitch,
        info->id, info->oauth_token, NULL, follows_all_cb, self);
}

void
gt_follows_manager_load_from_file(GtFollowsManager* self)
{
    GtFollowsManagerPrivate* priv = gt_follows_manager_get_instance_private(self);
    JsonParser* parse = json_parser_new();
    JsonNode* root;
    JsonArray* jarr;
    gchar* fp = FAV_CHANNELS_FILE;
    GError* err = NULL;

    g_signal_emit(self, sigs[SIG_STARTED_LOADING_FOLLOWS], 0);

    g_clear_pointer(&self->follow_channels,
                    (GDestroyNotify) gt_channel_list_free);

    if (!g_file_test(fp, G_FILE_TEST_EXISTS))
        goto finish;

    json_parser_load_from_file(parse, fp, &err);

    if (err)
    {
        g_warning("{GtFollowsManager} Error loading follow channels '%s'", err->message);
        goto finish;
    }

    root = json_parser_get_root(parse);
    jarr = json_node_get_array(root);

    for (GList* l = json_array_get_elements(jarr); l != NULL; l = l->next)
    {
        GtChannel* chan = GT_CHANNEL(json_gobject_deserialize(GT_TYPE_CHANNEL, l->data));

        g_assert(GT_IS_CHANNEL(chan));
        g_assert(g_object_is_floating(chan));

        g_object_ref_sink(chan);

        self->follow_channels = g_list_append(self->follow_channels, chan);
        g_signal_handlers_block_by_func(chan, channel_followed_cb, self);
        g_object_set(chan,
                     "auto-update", TRUE,
                     "followed", TRUE,
                     NULL);
        gt_channel_update(chan);
        g_signal_handlers_unblock_by_func(chan, channel_followed_cb, self);

        g_signal_connect(chan, "notify::updating", G_CALLBACK(oneshot_updating_cb), self);
    }

finish:

    g_signal_emit(self, sigs[SIG_FINISHED_LOADING_FOLLOWS], 0);

    g_object_unref(parse);
    g_free(fp);
}

void
gt_follows_manager_save(GtFollowsManager* self)
{
    GtFollowsManagerPrivate* priv = gt_follows_manager_get_instance_private(self);
    gchar* fp = FAV_CHANNELS_FILE;

    if (g_list_length(self->follow_channels) == 0)
    {
        if (g_file_test(fp, G_FILE_TEST_EXISTS))
        {
            g_remove(fp);
            g_free(fp);
        }

        return;
    }

    JsonArray* jarr = json_array_new();
    JsonGenerator* gen = json_generator_new();
    JsonNode* final = json_node_new(JSON_NODE_ARRAY);

    for (GList* l = self->follow_channels; l != NULL; l = l->next)
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
gt_follows_manager_is_channel_followed(GtFollowsManager* self, GtChannel* chan)
{
    return g_list_find_custom(self->follow_channels, chan, (GCompareFunc) gt_channel_compare) != NULL;
}

void
gt_follows_manager_attach_to_channel(GtFollowsManager* self, GtChannel* chan)
{
    g_signal_connect(chan, "notify::followed", G_CALLBACK(channel_followed_cb), self);
}
