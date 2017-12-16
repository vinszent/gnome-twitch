/*
 *  This file is part of GNOME Twitch - 'Enjoy Twitch on your GNU/Linux desktop'
 *  Copyright © 2017 Vincent Szolnoky <vinszent@vinszent.com>
 *
 *  GNOME Twitch is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GNOME Twitch is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNOME Twitch. If not, see <http://www.gnu.org/licenses/>.
 */

#include "gt-follows-manager.h"
#include "gt-app.h"
#include "gt-http.h"
#include "gt-win.h"
#include "utils.h"
#include <json-glib/json-glib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#define TAG "GtFollowsManager"
#include "gnome-twitch/gt-log.h"

#define OLD_FAV_CHANNELS_FILE g_build_filename(g_get_user_data_dir(), "gnome-twitch", "favourite-channels.json", NULL);
#define FAV_CHANNELS_FILE g_build_filename(g_get_user_data_dir(), "gnome-twitch", "followed-channels.json", NULL);

#define FOLLOWED_CHANNELS_FILE_VERSION 1

struct _GtFollowsManagerPrivate
{
    gboolean loading_follows;
    GCancellable* cancel;
    JsonParser* json_parser;
    gint current_offset;
    GtChannelList* loading_list;
};

G_DEFINE_TYPE_WITH_PRIVATE(GtFollowsManager, gt_follows_manager, G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_LOADING_FOLLOWS,
    NUM_PROPS
};

enum
{
    SIG_CHANNEL_FOLLOWED,
    SIG_CHANNEL_UNFOLLOWED,
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
    GParamSpec* pspec, gpointer udata)
{
    RETURN_IF_FAIL(GT_IS_CHANNEL(source));
    RETURN_IF_FAIL(GT_IS_FOLLOWS_MANAGER(udata));

    GtChannel* chan = GT_CHANNEL(source);

    if (gt_channel_is_online(chan) && gt_app_should_show_notifications(main_app))
    {
        g_autoptr(GNotification) notification;
        g_autofree gchar* title_str = NULL;
        g_autofree gchar* body_str = NULL;
        GVariant* var = NULL;

        title_str = g_strdup_printf(_("%s started streaming %s"),
            gt_channel_get_display_name(chan), gt_channel_get_game_name(chan));
        body_str = g_strdup_printf(_("Click here to start watching"));

        var = g_variant_new_string(gt_channel_get_id(chan));

        notification = g_notification_new(title_str);
        g_notification_set_body(notification, body_str);
        g_notification_set_default_action_and_target_value(notification,
            "app.open-channel-from-id", var);

        g_application_send_notification(G_APPLICATION(main_app), NULL, notification);
    }
}

static void
channel_followed_cb(GObject* source,
                      GParamSpec* pspec,
                      gpointer udata);
static gboolean
toggle_followed_cb(gpointer udata)
{
    RETURN_VAL_IF_FAIL(GT_IS_CHANNEL(udata), G_SOURCE_REMOVE);

    GtChannel* chan = GT_CHANNEL(udata);
    GtFollowsManager* self = main_app->fav_mgr;

    g_signal_handlers_block_by_func(chan, channel_followed_cb, self);
    gt_channel_toggle_followed(chan);
    g_signal_handlers_unblock_by_func(chan, channel_followed_cb, self);

    return G_SOURCE_REMOVE;
}

static void
channel_followed_cb(GObject* source,
    GParamSpec* pspec, gpointer udata)
{
    RETURN_IF_FAIL(GT_IS_FOLLOWS_MANAGER(udata));
    RETURN_IF_FAIL(GT_IS_CHANNEL(source));

    GtFollowsManager* self = GT_FOLLOWS_MANAGER(udata);
    GtChannel* chan = GT_CHANNEL(source);
    g_autoptr(GError) err = NULL;
    const gchar* name = gt_channel_get_name(chan);

    if (gt_channel_is_followed(chan))
    {
        if (gt_app_is_logged_in(main_app))
        {
            gt_twitch_follow_channel(main_app->twitch, name, &err); //TODO: Error handling
//            gt_twitch_follow_channel_async(main_app->twitch, gt_channel_get_name(chan), NULL, NULL); //TODO: Error handling

            if (err)
            {
                GtWin* win = NULL;

                WARNING("Unable to follow channel '%s' because: %s",
                    name, err->message);

                win = GT_WIN_ACTIVE;

                RETURN_IF_FAIL(GT_IS_WIN(win));

                gt_win_show_error_message(win, _("Unable to follow channel"),
                    "Unable to follow channel '%s' because: %s",
                    name, err->message);

                g_idle_add((GSourceFunc) toggle_followed_cb, chan);

                return;
            }
        }

        self->follow_channels = g_list_append(self->follow_channels, chan);
        g_signal_connect(chan, "notify::online", G_CALLBACK(channel_online_cb), self);
        g_object_ref(chan);

        MESSAGEF("Followed channel '%s'", name);

        g_signal_emit(self, sigs[SIG_CHANNEL_FOLLOWED], 0, chan);
    }
    else
    {
        GList* found = g_list_find_custom(self->follow_channels, chan, (GCompareFunc) gt_channel_compare);

        /* NOTE: This should never be NULL */
        RETURN_IF_FAIL(found != NULL);

        if (gt_app_is_logged_in(main_app))
        {
//            gt_twitch_unfollow_channel_async(main_app->twitch, gt_channel_get_name(chan), NULL, NULL); //TODO: Error handling
            gt_twitch_unfollow_channel(main_app->twitch, gt_channel_get_name(chan), &err);

            if (err)
            {
                GtWin* win = NULL;

                WARNING("Unable to unfollow channel '%s' because: %s",
                    name, err->message);

                win = GT_WIN_ACTIVE;

                RETURN_IF_FAIL(GT_IS_WIN(win));

                gt_win_show_error_message(win, _("Unable to unfollow channel"),
                    "Unable to unfollow channel '%s' because: %s",
                    name, err->message);

                g_idle_add((GSourceFunc) toggle_followed_cb, chan);

                return;
            }
        }

        g_signal_handlers_disconnect_by_func(found->data, channel_online_cb, self);

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
channel_updating_oneshot_cb(GObject* source,
    GParamSpec* pspec, gpointer udata)
{
    RETURN_IF_FAIL(GT_IS_FOLLOWS_MANAGER(udata));
    RETURN_IF_FAIL(GT_IS_CHANNEL(source));

    GtFollowsManager* self = GT_FOLLOWS_MANAGER(udata);
    GtChannel* chan = GT_CHANNEL(source);

    if (!gt_channel_is_updating(chan))
    {
        g_signal_connect(chan, "notify::online",
            G_CALLBACK(channel_online_cb), self);

        g_signal_handlers_disconnect_by_func(source,
            channel_updating_oneshot_cb, self);
    }
}

static void
shutdown_cb(GApplication* app,
            gpointer udata)
{
    GtFollowsManager* self = GT_FOLLOWS_MANAGER(udata);

//    gt_follows_manager_save(self);
}

static GList*
load_from_file(const char* filepath, GError** error)
{
    g_autoptr(JsonParser) parser = json_parser_new();
    g_autoptr(JsonReader) reader = NULL;
    g_autoptr(GError) err = NULL;
    GList* ret = NULL;

    if (!g_file_test(filepath, G_FILE_TEST_EXISTS))
    {
        MESSAGE("Follows file at '%s' doesn't exist", filepath);

        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
            "File '%s' does not exist", filepath);

        return NULL;
    }

    json_parser_load_from_file(parser, filepath, error);

    reader = json_reader_new(json_parser_get_root(parser));

    for (gint i = 0; i < json_reader_count_elements(reader); i++)
    {
        const gchar* id;
        const gchar* name;
        JsonNode* id_node = NULL;
        GtChannel* chan = NULL;

        if (!json_reader_read_element(reader, i))
        {
            *error = g_error_copy(json_reader_get_error(reader));

            goto error;
        }

        if (!json_reader_read_member(reader, "id"))
        {
            *error = g_error_copy(json_reader_get_error(reader));

            goto error;
        }

        id_node = json_reader_get_value(reader); //NOTE: Owned by JsonReader, don't unref

        //NOTE: Backwards compatability for when id's used to be integers
        if (STRING_EQUALS(json_node_type_name(id_node), "Integer"))
            id = g_strdup_printf("%" G_GINT64_FORMAT, json_reader_get_int_value(reader));
        else if (STRING_EQUALS(json_node_type_name(id_node), "String"))
            id = g_strdup(json_reader_get_string_value(reader));
        else
            goto error;

        json_reader_end_element(reader);

        if (!json_reader_read_member(reader, "name"))
        {
            *error = g_error_copy(json_reader_get_error(reader));

            goto error;
        }

        name = json_reader_get_string_value(reader);

        json_reader_end_member(reader);

        json_reader_end_element(reader);

        chan = gt_channel_new_from_id_and_name(id, name);

        g_object_ref_sink(chan);

        ret = g_list_append(ret, chan);
    }

    return ret;

error:
    gt_channel_list_free(ret);

    return ret;
}

static void
follow_next_channel_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    RETURN_IF_FAIL(G_IS_ASYNC_RESULT(res));
    RETURN_IF_FAIL(udata != NULL);

    GList* l = udata;
    g_autoptr(GError) err = NULL;
    g_task_propagate_pointer(G_TASK(res), &err); //NOTE: Doesn't return a value

    if (err)
    {
        GList* last = g_list_last(l);

        RETURN_IF_FAIL(GT_IS_FOLLOWS_MANAGER(last->data));

        GtFollowsManager* self = last->data;
        GtFollowsManagerPrivate* priv = gt_follows_manager_get_instance_private(udata);
        GtWin* win = NULL;

        win = GT_WIN_ACTIVE;

        RETURN_IF_FAIL(GT_IS_WIN(win));

        gt_win_show_error_message(win, _("Unable to move your local follows to Twitch"),
            "Unable to move your local follows to Twitch because: %s", err->message);

        priv->loading_follows = TRUE;
        g_object_notify_by_pspec(G_OBJECT(self), props[PROP_LOADING_FOLLOWS]);

        return;
    }

    if (GT_IS_CHANNEL(l->data))
    {
        gt_twitch_follow_channel_async(main_app->twitch,
            gt_channel_get_name(GT_CHANNEL(l->data)),
            follow_next_channel_cb, l->next);
    }
    else if (GT_IS_FOLLOWS_MANAGER(l->data))
    {
        g_autofree gchar* fp = FAV_CHANNELS_FILE;
        g_autofree gchar* new_fp = g_strconcat(fp, ".bak", NULL);

        g_rename(fp, new_fp);

        gt_follows_manager_load_from_twitch(GT_FOLLOWS_MANAGER(l->data));
    }
    else
        RETURN_IF_REACHED();
}

static void
move_local_follows_cb(GtkInfoBar* bar,
    gint res, gpointer udata)
{
    RETURN_IF_FAIL(GT_IS_FOLLOWS_MANAGER(udata));

    GtFollowsManager* self = GT_FOLLOWS_MANAGER(udata);

    if (res == GTK_RESPONSE_YES)
    {
        g_autofree gchar* filepath = FAV_CHANNELS_FILE;
        g_autoptr(GError) err = NULL;
        GList* channels = NULL;

        channels = load_from_file(filepath, &err);

        if (err)
        {
            GtWin* win;

            WARNING("Unable to move local follows to Twitch because: %s", err->message);

            win = GT_WIN_ACTIVE;

            RETURN_IF_FAIL(GT_IS_WIN(win));

            gt_win_show_error_message(win, _("Unable to move your local follows to Twitch"),
                "Unable to move local follows to Twitch because: %s", err->message);

            return;
        }

        channels = g_list_append(channels, self);

        gt_twitch_follow_channel_async(main_app->twitch,
            gt_channel_get_name(GT_CHANNEL(channels->data)),
            follow_next_channel_cb, channels->next);
    }
    else
    {
        g_autofree gchar* fp = FAV_CHANNELS_FILE;
        g_autofree gchar* new_fp = g_strconcat(fp, ".bak", NULL);

        g_rename(fp, new_fp);

        gt_follows_manager_load_from_twitch(self);
    }
}

static void
logged_in_cb(GObject* source,
             GParamSpec* pspec,
             gpointer udata)
{
    GtFollowsManager* self = GT_FOLLOWS_MANAGER(udata);

    if (gt_app_is_logged_in(main_app))
    {
        g_autofree gchar* filepath = FAV_CHANNELS_FILE;

        if (g_file_test(filepath, G_FILE_TEST_EXISTS))
        {
            gt_win_ask_question(GT_WIN_ACTIVE,
                _("GNOME Twitch has detected local follows, would you like to move them to Twitch?"),
                G_CALLBACK(move_local_follows_cb), self);
        }
        else
            gt_follows_manager_load_from_twitch(self);
    }
    else
        gt_follows_manager_load_from_file(self);
}

static void
handle_channels_response_cb(GtHTTP* http,
    gpointer res, GError* error, gpointer udata);

static void
process_channels_json_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    RETURN_IF_FAIL(JSON_IS_PARSER(source));
    RETURN_IF_FAIL(G_IS_ASYNC_RESULT(res));
    RETURN_IF_FAIL(udata != NULL);

    g_autoptr(GWeakRef) ref = udata;
    g_autoptr(GtFollowsManager) self = g_weak_ref_get(ref);

    if (!self)
    {
        TRACE("Not processing followed streams json because we were unreffed while waiting");
        return;
    }

    GtFollowsManagerPrivate* priv = gt_follows_manager_get_instance_private(self);
    g_autoptr(JsonReader) reader = NULL;
    g_autoptr(GError) err = NULL;
    gint num_elements;
    gint64 total;

    json_parser_load_from_stream_finish(priv->json_parser, res, &err);

    RETURN_IF_ERROR(err); /* FIXME: Handle error */

    reader = json_reader_new(json_parser_get_root(priv->json_parser));

    if (!json_reader_read_member(reader, "_total")) RETURN_IF_REACHED(); /* FIXME: Handle error */
    total = json_reader_get_int_value(reader);
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "follows")) RETURN_IF_REACHED(); /* FIXME: Handle error */

    num_elements = json_reader_count_elements(reader);

    for (gint i = 0; i < num_elements; i++)
    {
        GtChannelData* data = NULL;
        gboolean found = FALSE;

        if (!json_reader_read_element(reader, i)) RETURN_IF_REACHED(); /* FIXME: Handle error */
        if (!json_reader_read_member(reader, "channel")) RETURN_IF_REACHED(); /* FIXME: Handle error */

        data = utils_parse_channel_from_json(reader, &err);

        RETURN_IF_ERROR(err); /* FIXME: Handle error */

        for (GList* l = priv->loading_list; l != NULL; l = l->next)
        {
            GtChannel* chan = GT_CHANNEL(l->data);

            if (STRING_EQUALS(gt_channel_get_id(chan), data->id)
                && STRING_EQUALS(gt_channel_get_name(chan), data->name))
            {
                gt_channel_data_free(data);
                found = TRUE;

                break;
            }
        }

        if (!found)
            priv->loading_list = g_list_append(priv->loading_list, gt_channel_new(data));

        json_reader_end_member(reader);
        json_reader_end_element(reader);
    }

    json_reader_end_member(reader);

    priv->current_offset += num_elements;

    const GtOAuthInfo* info = gt_app_get_oauth_info(main_app);

    if (priv->current_offset < total)
    {
        g_autofree gchar* uri = NULL;

        uri = g_strdup_printf("https://api.twitch.tv/kraken/users/%s/follows/channels?limit=%d&offset=%d",
            info->user_id, 100, priv->current_offset);

        gt_http_get_with_category(main_app->http, uri, "gt-follows-manager", DEFAULT_TWITCH_HEADERS, priv->cancel,
            handle_channels_response_cb, g_steal_pointer(&ref), GT_HTTP_FLAG_RETURN_STREAM);
    }
    else
    {
        for (GList* l = priv->loading_list; l != NULL; l = l->next)
        {
            GtChannel* chan = l->data;

            RETURN_IF_FAIL(GT_IS_CHANNEL(chan));
            RETURN_IF_FAIL(g_object_is_floating(chan));

            g_object_ref_sink(chan);

            g_signal_connect(chan, "notify::updating",
                G_CALLBACK(channel_updating_oneshot_cb), self);

            g_signal_handlers_block_by_func(chan, channel_followed_cb, self);

            g_object_set(chan,
                "auto-update", TRUE,
                "followed", TRUE,
                NULL);
            g_signal_emit(self, sigs[SIG_CHANNEL_FOLLOWED], 0, chan);

            g_signal_handlers_unblock_by_func(chan, channel_followed_cb, self);
        }

        gt_channel_list_free(self->follow_channels);
        self->follow_channels = priv->loading_list;
        priv->loading_list = NULL;

        priv->loading_follows = FALSE;
        g_object_notify_by_pspec(G_OBJECT(self), props[PROP_LOADING_FOLLOWS]);

        MESSAGE("Loaded '%d' follows", g_list_length(self->follow_channels));
    }
}

static void
handle_channels_response_cb(GtHTTP* http,
    gpointer res, GError* error, gpointer udata)
{
    RETURN_IF_FAIL(GT_IS_HTTP(http));
    RETURN_IF_FAIL(udata != NULL);

    g_autoptr(GWeakRef) ref = udata;
    g_autoptr(GtFollowsManager) self = g_weak_ref_get(ref);

    if (!self) {TRACE("Unreffed while waiting"); return;}

    GtFollowsManagerPrivate* priv = gt_follows_manager_get_instance_private(self);

    RETURN_IF_ERROR(error); /* FIXME: Handle error */

    RETURN_IF_FAIL(G_IS_INPUT_STREAM(res));

    json_parser_load_from_stream_async(priv->json_parser, res, priv->cancel,
        process_channels_json_cb, g_steal_pointer(&ref));
}

static void
handle_streams_response_cb(GtHTTP* http,
    gpointer res, GError* error, gpointer udata);

static void
process_streams_json_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    RETURN_IF_FAIL(JSON_IS_PARSER(source));
    RETURN_IF_FAIL(G_IS_ASYNC_RESULT(res));
    RETURN_IF_FAIL(udata != NULL);

    g_autoptr(GWeakRef) ref = udata;
    g_autoptr(GtFollowsManager) self = g_weak_ref_get(ref);

    if (!self)
    {
        TRACE("Not processing followed channels json because we were unreffed while waiting");
        return;
    }

    GtFollowsManagerPrivate* priv = gt_follows_manager_get_instance_private(self);
    g_autoptr(JsonReader) reader = NULL;
    g_autoptr(GError) err = NULL;
    gint num_elements;
    gint64 total;

    json_parser_load_from_stream_finish(priv->json_parser, res, &err);

    RETURN_IF_ERROR(err); /* FIXME: Handle error */

    reader = json_reader_new(json_parser_get_root(priv->json_parser));

    if (!json_reader_read_member(reader, "_total")) RETURN_IF_REACHED(); /* FIXME: Handle error */
    total = json_reader_get_int_value(reader);
    json_reader_end_member(reader);

    if (!json_reader_read_member(reader, "streams")) RETURN_IF_REACHED(); /* FIXME: Handle error */

    num_elements = json_reader_count_elements(reader);

    for (gint i = 0; i < num_elements; i++)
    {
        json_reader_read_element(reader, i);

        priv->loading_list = g_list_append(priv->loading_list,
            gt_channel_new(utils_parse_stream_from_json(reader, &err)));

        RETURN_IF_ERROR(err); /* FIXME: Handle error */

        json_reader_end_element(reader);
    }

    json_reader_end_member(reader);

    priv->current_offset += num_elements;

    const GtOAuthInfo* info = gt_app_get_oauth_info(main_app);

    g_autofree gchar* uri = NULL;

    if (priv->current_offset < total)
    {
        uri = g_strdup_printf("https://api.twitch.tv/kraken/streams/followed?oauth_token=%s&limit=%d&offset=%d&stream_type=live",
            info->oauth_token, 100, priv->current_offset);

        gt_http_get_with_category(main_app->http, uri, "gt-follows-manager", DEFAULT_TWITCH_HEADERS, priv->cancel,
            handle_streams_response_cb, g_steal_pointer(&ref), GT_HTTP_FLAG_RETURN_STREAM);
    }
    else
    {
        priv->current_offset = 0;

        uri = g_strdup_printf("https://api.twitch.tv/kraken/users/%s/follows/channels?limit=%d&offset=%d",
            info->user_id, 100, 0);

        gt_http_get_with_category(main_app->http, uri, "gt-follows-manager", DEFAULT_TWITCH_HEADERS, priv->cancel,
            handle_channels_response_cb, g_steal_pointer(&ref), GT_HTTP_FLAG_RETURN_STREAM);
    }
}

static void
handle_streams_response_cb(GtHTTP* http,
    gpointer res, GError* error, gpointer udata)
{
    RETURN_IF_FAIL(GT_IS_HTTP(http));
    RETURN_IF_FAIL(udata != NULL);

    g_autoptr(GWeakRef) ref = udata;
    g_autoptr(GtFollowsManager) self = g_weak_ref_get(ref);

    if (!self) {TRACE("Unreffed while waiting"); return;}

    GtFollowsManagerPrivate* priv = gt_follows_manager_get_instance_private(self);

    RETURN_IF_ERROR(error); /* FIXME: Handle error */

    RETURN_IF_FAIL(G_IS_INPUT_STREAM(res));

    json_parser_load_from_stream_async(priv->json_parser, res, priv->cancel,
        process_streams_json_cb, g_steal_pointer(&ref));
}

static void
finalize(GObject* object)
{
    GtFollowsManager* self = (GtFollowsManager*) object;

    gt_channel_list_free(self->follow_channels);

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
        case PROP_LOADING_FOLLOWS:
            g_value_set_boolean(val, priv->loading_follows);
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
    GtFollowsManager* self = GT_FOLLOWS_MANAGER(obj);

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

    props[PROP_LOADING_FOLLOWS] = g_param_spec_boolean("loading-follows",
        "Loading follows", "Whether loading follows", FALSE, G_PARAM_READABLE);

    sigs[SIG_CHANNEL_FOLLOWED] = g_signal_new("channel-followed",
        GT_TYPE_FOLLOWS_MANAGER, G_SIGNAL_RUN_LAST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 1, GT_TYPE_CHANNEL);

    sigs[SIG_CHANNEL_UNFOLLOWED] = g_signal_new("channel-unfollowed",
        GT_TYPE_FOLLOWS_MANAGER, G_SIGNAL_RUN_LAST, 0,
        NULL, NULL, NULL, G_TYPE_NONE, 1, GT_TYPE_CHANNEL);
}

static void
gt_follows_manager_init(GtFollowsManager* self)
{
    g_assert(GT_IS_FOLLOWS_MANAGER(self));

    GtFollowsManagerPrivate* priv = gt_follows_manager_get_instance_private(self);

    priv->json_parser = json_parser_new();
    priv->loading_list = NULL;

    self->follow_channels = NULL;

    g_autofree gchar* old_fp = OLD_FAV_CHANNELS_FILE;
    g_autofree gchar* new_fp = FAV_CHANNELS_FILE;

    g_signal_connect(main_app, "shutdown", G_CALLBACK(shutdown_cb), self);
    g_signal_connect(main_app, "notify::logged-in", G_CALLBACK(logged_in_cb), self);

    //TODO: Remove this in a release or two
    if (g_file_test(old_fp, G_FILE_TEST_EXISTS))
        g_rename(old_fp, new_fp);
}

void
gt_follows_manager_load_from_twitch(GtFollowsManager* self)
{
    RETURN_IF_FAIL(GT_IS_FOLLOWS_MANAGER(self));

    GtFollowsManagerPrivate* priv = gt_follows_manager_get_instance_private(self);

    priv->loading_follows = TRUE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_LOADING_FOLLOWS]);

    const GtOAuthInfo* info = gt_app_get_oauth_info(main_app);
    g_autofree gchar* uri = NULL;

    utils_refresh_cancellable(&priv->cancel);

    priv->current_offset = 0;

    uri = g_strdup_printf("https://api.twitch.tv/kraken/streams/followed?oauth_token=%s&limit=%d&offset=%d&stream_type=live",
        info->oauth_token, 100, 0);

    gt_http_get_with_category(main_app->http, uri, "gt-follows-manager", DEFAULT_TWITCH_HEADERS, priv->cancel,
        handle_streams_response_cb, utils_weak_ref_new(self), GT_HTTP_FLAG_RETURN_STREAM);
}

void
gt_follows_manager_load_from_file(GtFollowsManager* self)
{
    RETURN_IF_FAIL(GT_IS_FOLLOWS_MANAGER(self));

    GtFollowsManagerPrivate* priv = gt_follows_manager_get_instance_private(self);
    g_autofree gchar* filepath = FAV_CHANNELS_FILE;
    g_autoptr(GError) err = NULL;

    priv->loading_follows = TRUE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_LOADING_FOLLOWS]);

    g_clear_pointer(&self->follow_channels,
        (GDestroyNotify) gt_channel_list_free);

    self->follow_channels = load_from_file(filepath, &err);

    if (err)
    {
        if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
            goto finish;
        else
        {
            GtWin* win = GT_WIN_ACTIVE;

            RETURN_IF_FAIL(GT_IS_WIN(win));

            WARNING("Error loading followed channels from file because: %s", err->message);

            gt_win_show_error_message(win, _("Unable to load followed channels from file"),
                "Unable to load followed channels from file because: %s", err->message);
        }

        return;
    }

    for (GList* l = self->follow_channels; l != NULL; l = l->next)
    {
        GtChannel* chan = l->data;

        RETURN_IF_FAIL(GT_IS_CHANNEL(chan));

        g_signal_handlers_block_by_func(chan, channel_followed_cb, self);

        g_object_set(chan,
            "auto-update", TRUE,
            "followed", TRUE,
            NULL);

        g_signal_handlers_unblock_by_func(chan, channel_followed_cb, self);

        g_signal_connect(chan, "notify::updating", G_CALLBACK(channel_updating_oneshot_cb), self);
    }

    MESSAGE("Loaded '%d' follows from file", g_list_length(self->follow_channels));

finish:
    priv->loading_follows = FALSE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_LOADING_FOLLOWS]);

    return;
}

void
gt_follows_manager_save(GtFollowsManager* self)
{
    RETURN_IF_FAIL(GT_IS_FOLLOWS_MANAGER(self));

    g_autofree gchar* fp = FAV_CHANNELS_FILE;

    MESSAGE("Saving follows to file '%s'", fp);

    if (g_list_length(self->follow_channels) == 0)
    {
        if (g_file_test(fp, G_FILE_TEST_EXISTS))
        {
            g_remove(fp);
            g_free(fp);
        }

        return;
    }

    g_autoptr(JsonBuilder) builder = json_builder_new();
    g_autoptr(JsonGenerator) gen = json_generator_new();
    g_autoptr(JsonNode) final = NULL;

    //NOTE: I'll just leave this here in case I want to include
    //a version tag in the future

    /* json_builder_begin_object(builder); */

    /* json_builder_set_member_name(builder, "version"); */
    /* json_builder_add_int_value(builder, FOLLOWED_CHANNELS_FILE_VERSION); */
    /* json_builder_end_object(builder); */

    /* json_builder_set_member_name(builder, "followed-channels"); */

    json_builder_begin_array(builder);

    for (GList* l = self->follow_channels; l != NULL; l = l->next)
    {
        json_builder_begin_object(builder);

        json_builder_set_member_name(builder, "id");
        json_builder_add_string_value(builder, gt_channel_get_id(l->data));

        json_builder_set_member_name(builder, "name");
        json_builder_add_string_value(builder, gt_channel_get_name(l->data));

        json_builder_end_object(builder);
    }

    json_builder_end_array(builder);

    /* json_builder_end_object(builder); */

    final = json_builder_get_root(builder);

    json_generator_set_root(gen, final);
    json_generator_to_file(gen, fp, NULL);
}

gboolean
gt_follows_manager_is_channel_followed(GtFollowsManager* self, GtChannel* chan)
{
    RETURN_VAL_IF_FAIL(GT_IS_FOLLOWS_MANAGER(self), FALSE);
    RETURN_VAL_IF_FAIL(GT_IS_CHANNEL(chan), FALSE);

    return g_list_find_custom(self->follow_channels, chan, (GCompareFunc) gt_channel_compare) != NULL;
}

gboolean
gt_follows_manager_is_loading_follows(GtFollowsManager* self)
{
    RETURN_VAL_IF_FAIL(GT_IS_FOLLOWS_MANAGER(self), FALSE);

    GtFollowsManagerPrivate* priv = gt_follows_manager_get_instance_private(self);

    return priv->loading_follows;
}

void
gt_follows_manager_attach_to_channel(GtFollowsManager* self, GtChannel* chan)
{
    RETURN_IF_FAIL(GT_IS_FOLLOWS_MANAGER(self));
    RETURN_IF_FAIL(GT_IS_CHANNEL(chan));

    g_signal_connect(chan, "notify::followed", G_CALLBACK(channel_followed_cb), self);
}

void
gt_follows_manager_refresh(GtFollowsManager* self)
{
    RETURN_IF_FAIL(GT_IS_FOLLOWS_MANAGER(self));

    if (gt_app_is_logged_in(main_app))
        gt_follows_manager_load_from_twitch(self);
    else
        g_list_foreach(self->follow_channels, (GFunc) gt_channel_update, NULL);
}
