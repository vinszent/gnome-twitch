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

#include "gt-channel.h"
#include "gt-app.h"
#include "gt-win.h"
#include <json-glib/json-glib.h>
#include <glib/gi18n.h>

#define TAG "GtChannel"
#include "gnome-twitch/gt-log.h"
#include "utils.h"

#define N_JSON_PROPS 2

typedef struct
{
    GtChannelData* data;

    GdkPixbuf* preview;

    gboolean followed;

    gboolean auto_update;
    gboolean updating;

    gboolean error;
    gchar* error_message;
    gchar* error_details;

    gchar* cache_filename;
    gint64 cache_timestamp;
    gint64 preview_timestamp;

    guint update_id;
    guint update_set_id;

    GCancellable* cancel;
} GtChannelPrivate;

static GThreadPool* update_pool;
static GThreadPool* cache_update_pool;

G_DEFINE_TYPE_WITH_CODE(GtChannel, gt_channel, G_TYPE_INITIALLY_UNOWNED,
    G_ADD_PRIVATE(GtChannel))

enum
{
    PROP_0,
    PROP_ID,
    PROP_STATUS,
    PROP_GAME,
    PROP_NAME,
    PROP_DISPLAY_NAME,
    PROP_PREVIEW_URL,
    PROP_VIDEO_BANNER_URL,
    PROP_LOGO_URL,
    PROP_PROFILE_URL,
    PROP_PREVIEW,
    PROP_VIEWERS,
    PROP_STREAM_STARTED_TIME,
    PROP_FOLLOWED,
    PROP_ONLINE,
    PROP_AUTO_UPDATE,
    PROP_UPDATING,
    PROP_ERROR,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

static inline void
set_banner(GtChannel* self, GdkPixbuf* banner, gboolean save)
{
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    priv->preview = banner;

    if (save)
    {
        gdk_pixbuf_save(priv->preview, priv->cache_filename,
                        "jpeg", NULL, NULL);
    }

    utils_pixbuf_scale_simple(&priv->preview,
        320, 180, GDK_INTERP_BILINEAR);

    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_PREVIEW]);
}

static void
channel_followed_cb(GtFollowsManager* mgr,
                      GtChannel* chan,
                      gpointer udata)
{
    GtChannel* self = GT_CHANNEL(udata);
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    if (!gt_channel_compare(self, chan) && !priv->followed)
    {
        GQuark detail = g_quark_from_static_string("followed");
        g_signal_handlers_block_matched(self, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_DETAIL, 0, detail, NULL, NULL, main_app->fav_mgr);
        g_object_set(self, "followed", TRUE, NULL);
        g_signal_handlers_unblock_matched(self, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_DETAIL, 0, detail, NULL, NULL, main_app->fav_mgr);
    }
}

static void
channel_unfollowed_cb(GtFollowsManager* mgr,
                        GtChannel* chan,
                        gpointer udata)
{
    GtChannel* self = GT_CHANNEL(udata);
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    if (!gt_channel_compare(self, chan) && priv->followed)
    {
        GQuark detail = g_quark_from_static_string("followed");
        g_signal_handlers_block_matched(self, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_DETAIL, 0, detail, NULL, NULL, main_app->fav_mgr);
        g_object_set(self, "followed", FALSE, NULL);
        g_signal_handlers_unblock_matched(self, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_DETAIL, 0, detail, NULL, NULL, main_app->fav_mgr);
    }
}


static void
auto_update_cb(GObject* src,
               GParamSpec* pspec,
               gpointer udata)
{
    GtChannel* self = GT_CHANNEL(src);
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    if (priv->auto_update)
        priv->update_id = g_timeout_add_seconds(120, (GSourceFunc) gt_channel_update, self); //TODO: Add this as a setting
    else
        g_source_remove(priv->update_id);
}

static void
cache_update_cb(gpointer data,
                gpointer udata)
{
    g_assert(GT_IS_CHANNEL(data));

    g_autoptr(GtChannel) self = data;
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);
    g_autoptr(GError) err = NULL;

    GdkPixbuf* pic = gt_twitch_download_picture(main_app->twitch,
        priv->data->video_banner_url, priv->cache_timestamp, &err);

    if (err)
    {
        WARNING("Unable to update cache for channel with name '%s'", priv->data->name);

        priv->error_message = g_strdup("Unable to update cache");
        priv->error_details = g_strdup_printf("Unable to update cache for channel with name '%s' because: %s",
            priv->data->name, err->message);

        priv->error = TRUE;
        g_object_notify_by_pspec(G_OBJECT(self), props[PROP_ERROR]);

        return;
    }

    if (pic)
    {
        set_banner(self, pic, TRUE);

        INFO("Updated cache for channel with name '%s'", priv->data->name);
    }
}

static void
download_preview_cb(GObject* source,
                    GAsyncResult* res,
                    gpointer udata)
{
    g_assert(GT_IS_CHANNEL(udata));

    g_autoptr(GtChannel) self = udata;
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);
    g_autoptr(GError) err = NULL;

    GdkPixbuf* pic = g_task_propagate_pointer(G_TASK(res), &err);

    if (err)
    {
        WARNING("Unable to update preview for channel with name '%s'",
            priv->data->name);

        priv->error_message = g_strdup("Unable to update preview");
        priv->error_details = g_strdup_printf("Unable to update preview for channel with name '%s' because: %s",
            priv->data->name, err->message);

        priv->error = TRUE;
        g_object_notify_by_pspec(G_OBJECT(self), props[PROP_ERROR]);

        return;
    }

    if (pic)
    {
        g_clear_object(&priv->preview);
        priv->preview_timestamp = utils_timestamp_now();
        priv->preview = pic;
        utils_pixbuf_scale_simple(&priv->preview,
            320, 180, GDK_INTERP_BILINEAR);
        g_object_notify_by_pspec(G_OBJECT(self), props[PROP_PREVIEW]);
    }

    priv->updating = FALSE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_UPDATING]);
}

static void
download_banner_cb(GObject* source,
                   GAsyncResult* res,
                   gpointer udata)
{
    g_assert(GT_IS_CHANNEL(udata));

    g_autoptr(GtChannel) self = GT_CHANNEL(udata);
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);
    g_autoptr(GError) err = NULL;

    GdkPixbuf* pic = g_task_propagate_pointer(G_TASK(res), &err);

    if (err)
    {
        if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
            DEBUG("Updating banner cancelled");

            goto finish;
        }

        WARNING("Unable to update banner for channel with name '%s' because; %s",
            priv->data->name, err->message);

        priv->error_message = g_strdup("Unable to update banner");
        priv->error_details = g_strdup_printf("Unable to update banner for channel with name '%s' because: %s",
            priv->data->name, err->message);

        priv->error = TRUE;
        g_object_notify_by_pspec(G_OBJECT(self), props[PROP_ERROR]);

        return;
    }

    set_banner(self, pic, TRUE);

finish:
    priv->updating = FALSE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_UPDATING]);
}

static void
download_banner(GtChannel* self)
{
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    if (priv->data->video_banner_url)
    {
        GdkPixbuf* banner = NULL;

        if (!g_file_test(priv->cache_filename, G_FILE_TEST_EXISTS))
            INFO("Chache miss for channel with name '%s'", priv->data->name);
        else
        {
            INFO("Cache hit for channel with name '%s'", priv->data->name);
            banner = gdk_pixbuf_new_from_file(priv->cache_filename, NULL);
            priv->cache_timestamp = utils_timestamp_file(priv->cache_filename);
        }

        if (!banner)
        {
            gt_twitch_download_picture_async(main_app->twitch, priv->data->video_banner_url,
                0, priv->cancel, download_banner_cb, g_object_ref(self));
        }
        else
        {
            g_thread_pool_push(cache_update_pool, g_object_ref(self), NULL);

            set_banner(self, banner, FALSE);

            priv->updating = FALSE;
            g_object_notify_by_pspec(G_OBJECT(self), props[PROP_UPDATING]);
        }
    }
    else
    {
        set_banner(self, gdk_pixbuf_new_from_resource("/com/vinszent/GnomeTwitch/icons/offline.png", NULL),
                   FALSE);

        priv->updating = FALSE;
        g_object_notify_by_pspec(G_OBJECT(self), props[PROP_UPDATING]);
    }
}

static void
update_preview(GtChannel* self)
{
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    utils_refresh_cancellable(&priv->cancel);

    if (priv->data->online)
    {
        gt_twitch_download_picture_async(main_app->twitch, priv->data->preview_url,
            priv->preview_timestamp, priv->cancel, download_preview_cb, g_object_ref(self));
    }
    else
        download_banner(self);
}

static void
update_from_data(GtChannel* self, GtChannelData* data)
{
    g_assert(GT_IS_CHANNEL(self));
    g_assert_nonnull(data);

    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    g_autoptr(GtChannelData) old_data = priv->data;

    priv->updating = TRUE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_UPDATING]);

    priv->data = data;

    if (old_data)
    {
        if (!STRING_EQUALS(old_data->id, data->id))
        {
            WARNING("Unable to update channel with id '%s' and name '%s' because: "
                "New data with id '%s' does not match the current one",
                old_data->id, old_data->name, data->id);

            priv->error_message = g_strdup("Unable to update data");
            priv->error_details = g_strdup_printf("Unable to update data for channel with id '%s' and name '%s' because: "
                "New data with id '%s' does not match the current one",
                old_data->id, old_data->name, data->id);

            priv->error = TRUE;
            g_object_notify_by_pspec(G_OBJECT(self), props[PROP_ERROR]);

            return;
        }

        if (!STRING_EQUALS(old_data->name, data->name))
        {
            WARNING("Unable to update channel with id '%s' and name '%s' because: "
                "New data with name '%s' does not match the current one",
                old_data->id, old_data->name, data->name);

            priv->error_message = g_strdup("Unable to update data");
            priv->error_details = g_strdup_printf("Unable to update data for channel with id '%s' and name '%s' because: "
                "New data with name '%s' does not match the current one",
                old_data->id, old_data->name, data->name);

            priv->error = TRUE;
            g_object_notify_by_pspec(G_OBJECT(self), props[PROP_ERROR]);

            return;
        }

        if (!STRING_EQUALS(old_data->game, data->game))
            g_object_notify_by_pspec(G_OBJECT(self), props[PROP_GAME]);
        if (!STRING_EQUALS(old_data->status, data->status))
            g_object_notify_by_pspec(G_OBJECT(self), props[PROP_STATUS]);
        if (!STRING_EQUALS(old_data->display_name, data->display_name))
            g_object_notify_by_pspec(G_OBJECT(self), props[PROP_DISPLAY_NAME]);
        if (!STRING_EQUALS(old_data->preview_url, data->preview_url))
            g_object_notify_by_pspec(G_OBJECT(self), props[PROP_PREVIEW_URL]);
        if (!STRING_EQUALS(old_data->video_banner_url, data->video_banner_url))
            g_object_notify_by_pspec(G_OBJECT(self), props[PROP_VIDEO_BANNER_URL]);
        if (!STRING_EQUALS(old_data->logo_url, data->logo_url))
            g_object_notify_by_pspec(G_OBJECT(self), props[PROP_LOGO_URL]);
        if (!STRING_EQUALS(old_data->profile_url, data->profile_url))
            g_object_notify_by_pspec(G_OBJECT(self), props[PROP_PROFILE_URL]);
        if (old_data->online != data->online)
            g_object_notify_by_pspec(G_OBJECT(self), props[PROP_ONLINE]);
        if (old_data->viewers != data->viewers)
            g_object_notify_by_pspec(G_OBJECT(self), props[PROP_VIEWERS]);
        if (data->stream_started_time && old_data->stream_started_time &&
            g_date_time_compare(old_data->stream_started_time, data->stream_started_time) != 0)
            g_object_notify_by_pspec(G_OBJECT(self), props[PROP_STREAM_STARTED_TIME]);
    }

    update_preview(self);
}


static gboolean
update_set_cb(gpointer udata)
{
    g_autoptr(GtChannel) self = GT_CHANNEL(udata);
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);
    GtChannelData* data = g_object_get_data(G_OBJECT(self), "data");

    if (!data)
    {
        WARNING("Unable to set update data");

        goto finish;
    }

    INFOF("Finished update '%s'", data->name);

    update_from_data(self, data);

    g_object_set_data(G_OBJECT(self), "data", NULL);

finish:
    priv->update_set_id = 0;

    return G_SOURCE_REMOVE;
}

static void
update_cb(gpointer data,
          gpointer udata)
{
    g_assert(GT_IS_CHANNEL(data));

    g_autoptr(GtChannel) self = data;
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);
    g_autoptr(GError) err = NULL;

    GtChannelData* chan_data = gt_twitch_fetch_channel_data(main_app->twitch, priv->data->id, &err);

    if (err)
    {
        WARNING("Unable to fetch channel data because: %s", err->message);

        priv->error_message = g_strdup("Unable to fetch data");
        priv->error_details = g_strdup_printf("Unable to fetch data for channel name '%s' because: %s",
            priv->data->name, err->message);

        priv->error = TRUE;
        g_object_notify_by_pspec(G_OBJECT(self), props[PROP_ERROR]);

        return;
    }

    g_object_set_data(G_OBJECT(self), "data", chan_data);

    priv->update_set_id = g_idle_add((GSourceFunc) update_set_cb, g_object_ref(self)); //Needs to be run on main thread.
}

static void
dispose(GObject* object)
{
    GtChannel* self = GT_CHANNEL(object);
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    g_cancellable_cancel(priv->cancel);

    g_clear_object(&priv->cancel);
    g_clear_object(&priv->preview);

    G_OBJECT_CLASS(gt_channel_parent_class)->dispose(object);
}

static void
finalize(GObject* object)
{
    GtChannel* self = (GtChannel*) object;
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    gt_channel_data_free(priv->data);

    if (priv->update_id > 0)
        g_source_remove(priv->update_id);

    g_signal_handlers_disconnect_by_func(main_app->fav_mgr, channel_followed_cb, self);
    g_signal_handlers_disconnect_by_func(main_app->fav_mgr, channel_unfollowed_cb, self);

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
            g_value_set_string(val, priv->data->id);
            break;
        case PROP_STATUS:
            g_value_set_string(val, priv->data->status);
            break;
        case PROP_NAME:
            g_value_set_string(val, priv->data->name);
            break;
        case PROP_DISPLAY_NAME:
            g_value_set_string(val, priv->data->display_name);
            break;
        case PROP_GAME:
            g_value_set_string(val, priv->data->game);
            break;
        case PROP_PREVIEW_URL:
            g_value_set_string(val, priv->data->preview_url);
            break;
        case PROP_VIDEO_BANNER_URL:
            g_value_set_string(val, priv->data->video_banner_url);
            break;
        case PROP_LOGO_URL:
            g_value_set_string(val, priv->data->logo_url);
            break;
        case PROP_PROFILE_URL:
            g_value_set_string(val, priv->data->profile_url);
            break;
        case PROP_VIEWERS:
            g_value_set_int64(val, priv->data->viewers);
            break;
        case PROP_STREAM_STARTED_TIME:
            g_value_set_pointer(val,
                priv->data->stream_started_time ?
                g_date_time_ref(priv->data->stream_started_time) : NULL);
            break;
        case PROP_ONLINE:
            g_value_set_boolean(val, priv->data->online);
            break;
        case PROP_FOLLOWED:
            g_value_set_boolean(val, priv->followed);
            break;
        case PROP_AUTO_UPDATE:
            g_value_set_boolean(val, priv->auto_update);
            break;
        case PROP_UPDATING:
            g_value_set_boolean(val, priv->updating);
            break;
        case PROP_PREVIEW:
            g_value_set_object(val, priv->preview);
            break;
        case PROP_ERROR:
            g_value_set_boolean(val, priv->error);
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
        case PROP_FOLLOWED:
            priv->followed = g_value_get_boolean(val);
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
    object_class->dispose = dispose;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_ID] = g_param_spec_string("id", "ID", "ID of channel",
        NULL, G_PARAM_READABLE);

    props[PROP_NAME] = g_param_spec_string("name", "Name", "Name of channel",
        NULL, G_PARAM_READABLE);

    props[PROP_STATUS] = g_param_spec_string("status", "Status", "Status of channel",
        NULL, G_PARAM_READABLE);

    props[PROP_DISPLAY_NAME] = g_param_spec_string("display-name", "Display Name", "Display Name of channel",
        NULL, G_PARAM_READABLE);

    props[PROP_GAME] = g_param_spec_string("game", "Game", "Game being played by channel",
        NULL, G_PARAM_READABLE);

    props[PROP_PREVIEW_URL] = g_param_spec_string("preview-url", "Preview Url", "Url for preview image",
        NULL, G_PARAM_READABLE);

    props[PROP_VIDEO_BANNER_URL] = g_param_spec_string("video-banner-url", "Video Banner Url", "Url for video banner image",
        NULL, G_PARAM_READABLE);

    props[PROP_LOGO_URL] = g_param_spec_string("logo-url", "Logo Url", "Url for logo",
        NULL, G_PARAM_READABLE);

    props[PROP_PROFILE_URL] = g_param_spec_string("profile-url", "Profile Url", "Url for profile",
        NULL, G_PARAM_READABLE);

    props[PROP_VIEWERS] = g_param_spec_int64("viewers", "Viewers", "Number of viewers",
        0, G_MAXINT64, 0, G_PARAM_READABLE);

    props[PROP_PREVIEW] = g_param_spec_object("preview", "Preview", "Preview of channel",
        GDK_TYPE_PIXBUF, G_PARAM_READABLE);

    //TODO: Spec this as a boxed type instead
    props[PROP_STREAM_STARTED_TIME] = g_param_spec_pointer("stream-started-time", "Stream started time", "Stream started time",
        G_PARAM_READABLE);

    props[PROP_ONLINE] = g_param_spec_boolean("online", "Online", "Whether the channel is online",
        TRUE, G_PARAM_READABLE);

    props[PROP_UPDATING] = g_param_spec_boolean("updating", "Updating", "Whether updating",
        FALSE, G_PARAM_READABLE);

    props[PROP_FOLLOWED] = g_param_spec_boolean("followed", "Followed", "Whether the channel is followed",
        FALSE, G_PARAM_READWRITE);

    props[PROP_AUTO_UPDATE] = g_param_spec_boolean("auto-update", "Auto Update", "Whether it should update itself automatically",
        FALSE, G_PARAM_READWRITE);

    props[PROP_ERROR] = g_param_spec_boolean("error", "Error", "Whether in error state",
        FALSE, G_PARAM_READABLE);

    g_object_class_install_properties(object_class, NUM_PROPS, props);

    update_pool = g_thread_pool_new((GFunc) update_cb, NULL, 2, FALSE, NULL);
    cache_update_pool = g_thread_pool_new((GFunc) cache_update_cb, NULL, 1, FALSE, NULL);
}


static void
gt_channel_init(GtChannel* self)
{
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    priv->data = NULL;
    priv->updating = FALSE;
    priv->cancel = g_cancellable_new();

    priv->update_id = 0;
    priv->update_set_id = 0;

    priv->error_message = NULL;
    priv->error_details = NULL;

    g_signal_connect(self, "notify::auto-update", G_CALLBACK(auto_update_cb), NULL);
    g_signal_connect(main_app->fav_mgr, "channel-followed", G_CALLBACK(channel_followed_cb), self);
    g_signal_connect(main_app->fav_mgr, "channel-unfollowed", G_CALLBACK(channel_unfollowed_cb), self);

    gt_follows_manager_attach_to_channel(main_app->fav_mgr, self);
}

GtChannel*
gt_channel_new(GtChannelData* data)
{
    g_assert_nonnull(data);

    GtChannel* channel = g_object_new(GT_TYPE_CHANNEL, NULL);
    GtChannelPrivate* priv = gt_channel_get_instance_private(channel);

    priv->cache_filename = g_build_filename(g_get_user_cache_dir(),
        "gnome-twitch", "channels", data->id, NULL);
    priv->cache_timestamp = utils_timestamp_file(priv->cache_filename);

    update_from_data(channel, data);

    priv->followed = gt_follows_manager_is_channel_followed(main_app->fav_mgr, channel);

    return channel;
}

GtChannel*
gt_channel_new_from_id_and_name(const gchar* id, const gchar* name)
{
    g_assert_false(utils_str_empty(id));

    GtChannel* channel = g_object_new(GT_TYPE_CHANNEL, NULL);
    GtChannelPrivate* priv = gt_channel_get_instance_private(channel);
    GtChannelData* data = gt_channel_data_new();

    data->id = g_strdup(id);
    data->name = g_strdup(name);

    priv->data = data;

    priv->cache_filename = g_build_filename(g_get_user_cache_dir(),
        "gnome-twitch", "channels", data->id, NULL);
    priv->cache_timestamp = utils_timestamp_file(priv->cache_filename);

    gt_channel_update(channel);

    priv->followed = gt_follows_manager_is_channel_followed(main_app->fav_mgr, channel);

    return channel;
}

void
gt_channel_toggle_followed(GtChannel* self)
{
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    g_object_set(self, "followed", !priv->followed, NULL);
}

void
gt_channel_list_free(GList* list)
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
        GtChannelPrivate* opriv = gt_channel_get_instance_private(GT_CHANNEL(other));

        ret = !(STRING_EQUALS(priv->data->name, opriv->data->name) &&
            STRING_EQUALS(priv->data->id, opriv->data->id));
    }

    return ret;
}

const gchar*
gt_channel_get_name(GtChannel* self)
{
    g_assert(GT_IS_CHANNEL(self));

    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    return priv->data->name;
}

const gchar*
gt_channel_get_id(GtChannel* self)
{
    g_assert(GT_IS_CHANNEL(self));

    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    return priv->data->id;
}

gboolean
gt_channel_is_online(GtChannel* self)
{
    g_assert(GT_IS_CHANNEL(self));

    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    return priv->data->online;
}

gboolean
gt_channel_is_error(GtChannel* self)
{
    g_assert(GT_IS_CHANNEL(self));

    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    return priv->error;
}

gboolean
gt_channel_is_updating(GtChannel* self)
{
    g_assert(GT_IS_CHANNEL(self));

    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    return priv->updating;
}

gboolean
gt_channel_update(GtChannel* self)
{
    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    INFO("Initiating update for channel with id '%s' and name '%s'",
        priv->data->id, priv->data->name);

    g_free(priv->error_message);
    g_free(priv->error_details);

    priv->error = FALSE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_ERROR]);

    priv->updating = TRUE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_UPDATING]);

    g_thread_pool_push(update_pool, g_object_ref(self), NULL);

    return TRUE;
}

const gchar*
gt_channel_get_error_message(GtChannel* self)
{
    g_assert(GT_IS_CHANNEL(self));

    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    return priv->error_message;
}

const gchar*
gt_channel_get_error_details(GtChannel* self)
{
    g_assert(GT_IS_CHANNEL(self));

    GtChannelPrivate* priv = gt_channel_get_instance_private(self);

    return priv->error_details;
}

GtChannelData*
gt_channel_data_new()
{
    return g_slice_new0(GtChannelData);
}

void
gt_channel_data_free(GtChannelData* data)
{
    if (!data) return;

    g_free(data->name);
    g_free(data->id);
    g_free(data->game);
    g_free(data->status);
    g_free(data->display_name);
    g_free(data->preview_url);
    g_free(data->video_banner_url);
    if (data->stream_started_time)
        g_date_time_unref(data->stream_started_time);
    g_slice_free(GtChannelData, data);
}

void
gt_channel_data_list_free(GList* list)
{
    g_list_free_full(list, (GDestroyNotify) gt_channel_data_free);
}

gint
gt_channel_data_compare(GtChannelData* a, GtChannelData* b)
{
    if (!a || !b)
        return -1;

    if (STRING_EQUALS(a->id, b->id))
        return 0;

    return -1;
}
