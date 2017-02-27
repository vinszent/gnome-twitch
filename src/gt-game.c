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

#include "gt-game.h"
#include "gt-app.h"
#include "gt-win.h"
#include "utils.h"
#include <glib/gi18n.h>

#define TAG "GtGame"
#include "gnome-twitch/gt-log.h"

typedef struct
{
    GtGameData* data;

    gboolean updating;

    gchar* preview_filename;
    gint64 preview_timestamp;

    GdkPixbuf* preview;

    GdkPixbuf* logo;

    GCancellable* cancel;
} GtGamePrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtGame, gt_game, G_TYPE_INITIALLY_UNOWNED)

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

static GThreadPool* cache_update_pool;

static GParamSpec* props[NUM_PROPS];

static inline void
set_preview(GtGame* self, GdkPixbuf* pic, gboolean save)
{
    g_assert(GT_IS_GAME(self));
    g_assert(GDK_IS_PIXBUF(pic));

    GtGamePrivate* priv = gt_game_get_instance_private(self);

    g_clear_object(&priv->preview);
    priv->preview = pic;

    if (save)
    {
        gdk_pixbuf_save(priv->preview, priv->preview_filename,
            "jpeg", NULL, NULL);
    }

    utils_pixbuf_scale_simple(&priv->preview,
        200, 270, GDK_INTERP_BILINEAR);

    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_PREVIEW]);
}

static void
cache_update_func(gpointer data, gpointer udata)
{
    g_assert(GT_IS_GAME(data));

    g_autoptr(GtGame) self = data;
    GtGamePrivate* priv = gt_game_get_instance_private(self);
    g_autoptr(GError) err = NULL;

    GdkPixbuf* pic = gt_twitch_download_picture(main_app->twitch,
        priv->data->preview_url, priv->preview_timestamp, &err);

    if (err)
    {
        WARNING("Unable to update preview for game '%s'", priv->data->name);

        //TODO: Put game in some kind of error state
        return;
    }

    if (pic)
    {
        set_preview(self, pic, TRUE);

        INFO("Updated preview for game '%s'", priv->data->name);
    }
}

static void
download_preview_cb(GObject* source,
                    GAsyncResult* res,
                    gpointer udata)
{
    g_assert(GT_IS_GAME(udata));

    g_autoptr(GtGame) self = udata;
    GtGamePrivate* priv = gt_game_get_instance_private(self);
    GError* error = NULL;

    GdkPixbuf* pic = g_task_propagate_pointer(G_TASK(res), &error);

    if (error)
        g_error_free(error);
    else
        set_preview(self, pic, TRUE);

    priv->updating = FALSE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_UPDATING]);

    g_object_unref(self);
}

static void
update_preview(GtGame* self)
{
    g_assert(GT_IS_GAME(self));

    GtGamePrivate* priv = gt_game_get_instance_private(self);

    if (g_file_test(priv->preview_filename, G_FILE_TEST_EXISTS))
    {
        priv->preview_timestamp = utils_timestamp_file(priv->preview_filename);
        set_preview(self, gdk_pixbuf_new_from_file(priv->preview_filename, NULL), FALSE);

        priv->updating = FALSE;
        g_object_notify_by_pspec(G_OBJECT(self), props[PROP_UPDATING]);

    }

    g_object_ref(self);

    if (!priv->preview)
    {
        g_print("Url %s\n", priv->data->preview_url);

        gt_twitch_download_picture_async(main_app->twitch,
            priv->data->preview_url, 0, priv->cancel,
            download_preview_cb, g_object_ref(self));
    }
    else
        g_thread_pool_push(cache_update_pool, g_object_ref(self), NULL);
}

static void
update_from_data(GtGame* self, GtGameData* data)
{
    g_assert(GT_IS_GAME(self));
    g_assert_nonnull(data);

    GtGamePrivate* priv = gt_game_get_instance_private(self);

    priv->updating = TRUE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_UPDATING]);

    g_autoptr(GtGameData) old_data = priv->data;

    priv->data = data;

    if (old_data)
    {
        if (!STRING_EQUALS(old_data->id, data->id))
        {
            GtWin* win = GT_WIN_ACTIVE;

            WARNING("Unable to update channel with id '%s' and name '%s' because: "
                "New data with id '%s' does not match the current one",
                old_data->id, old_data->name, data->id);

            if (GT_IS_WIN(win))
            {
                gt_win_show_error_message(win, "Unable to update channel '%s'",
                    "Unable to update channel with id '%s' and name '%s' because: "
                    "New data with id '%s' does not match the current one",
                    old_data->id, old_data->name, data->id);
            }

            return;
        }
    }

    if (!old_data || !STRING_EQUALS(old_data->preview_url, data->preview_url))
        g_object_notify_by_pspec(G_OBJECT(self), props[PROP_PREVIEW_URL]);
    if (!old_data || old_data->viewers != data->viewers)
        g_object_notify_by_pspec(G_OBJECT(self), props[PROP_VIEWERS]);
    if (!old_data || old_data->channels != data->channels)
        g_object_notify_by_pspec(G_OBJECT(self), props[PROP_CHANNELS]);

    update_preview(self);
}

static void
dispose(GObject* object)
{
    GtGame* self = GT_GAME(object);
    GtGamePrivate* priv = gt_game_get_instance_private(self);

    g_clear_object(&priv->preview);
    g_clear_object(&priv->logo);

    G_OBJECT_CLASS(gt_game_parent_class)->dispose(object);
}

static void
finalize(GObject* object)
{
    GtGame* self = (GtGame*) object;
    GtGamePrivate* priv = gt_game_get_instance_private(self);

    gt_game_data_free(priv->data);

    g_free(priv->preview_filename);

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
            g_value_set_string(val, priv->data->id);
            break;
        case PROP_NAME:
            g_value_set_string(val, priv->data->name);
            break;
        case PROP_UPDATING:
            g_value_set_boolean(val, priv->updating);
            break;
        case PROP_PREVIEW_URL:
            g_value_set_string(val, priv->data->preview_url);
            break;
        case PROP_PREVIEW:
            g_value_set_object(val, priv->preview);
            break;
        case PROP_LOGO:
            g_value_set_object(val, priv->logo);
            break;
        case PROP_VIEWERS:
            g_value_set_int64(val, priv->data->viewers);
            break;
        case PROP_CHANNELS:
            g_value_set_int64(val, priv->data->channels);
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
    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_game_class_init(GtGameClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->dispose = dispose;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_ID] = g_param_spec_string("id", "Id", "Id of game",
        NULL, G_PARAM_READABLE);

    props[PROP_NAME] = g_param_spec_string("name", "Name", "Name of game",
        NULL, G_PARAM_READABLE);

    props[PROP_UPDATING] = g_param_spec_boolean("updating", "Updating", "Whether updating",
        FALSE, G_PARAM_READABLE);

    props[PROP_PREVIEW_URL] = g_param_spec_string("preview-url", "Preview Url", "Current preview url",
        NULL, G_PARAM_READABLE);

    props[PROP_PREVIEW] = g_param_spec_object("preview", "Preview", "Preview of game",
        GDK_TYPE_PIXBUF, G_PARAM_READABLE);

    props[PROP_LOGO] = g_param_spec_object("logo", "Logo", "Logo of game",
        GDK_TYPE_PIXBUF, G_PARAM_READABLE);

    props[PROP_VIEWERS] = g_param_spec_int64("viewers", "Viewers", "Total viewers of game",
        0, G_MAXINT64, 0, G_PARAM_READABLE);

    props[PROP_CHANNELS] = g_param_spec_int64("channels", "Channels", "Total channels of game",
        0, G_MAXINT64, 0, G_PARAM_READABLE);

    g_object_class_install_properties(object_class,
                                      NUM_PROPS,
                                      props);

    cache_update_pool = g_thread_pool_new(cache_update_func, NULL, 1, FALSE, NULL);
}

static void
gt_game_init(GtGame* self)
{
    GtGamePrivate* priv = gt_game_get_instance_private(self);

    priv->updating = FALSE;
    priv->preview = NULL;
    priv->logo = NULL;
}

GtGame*
gt_game_new(GtGameData* data)
{
    g_assert_nonnull(data);

    GtGame* game = g_object_new(GT_TYPE_GAME, NULL);
    GtGamePrivate* priv = gt_game_get_instance_private(game);

    priv->preview_filename = g_build_filename(g_get_user_cache_dir(), "gnome-twitch",
        "games", data->id, NULL);
    priv->preview_timestamp = utils_timestamp_file(priv->preview_filename);

    update_from_data(game, data);

    return game;
}

void
gt_game_list_free(GList* list)
{
    g_list_free_full(list, g_object_unref);
}

const gchar*
gt_game_get_name(GtGame* self)
{
    g_assert(GT_IS_GAME(self));

    GtGamePrivate* priv = gt_game_get_instance_private(self);

    return priv->data->name;
}

gboolean
gt_game_get_updating(GtGame* self)
{
    g_assert(GT_IS_GAME(self));

    GtGamePrivate* priv = gt_game_get_instance_private(self);

    return priv->updating;
}

GtGameData*
gt_game_data_new()
{
    return g_slice_new0(GtGameData);
}

void
gt_game_data_free(GtGameData* data)
{
    if (!data) return;

    g_free(data->name);
    g_free(data->preview_url);
    g_free(data->logo_url);
    g_slice_free(GtGameData, data);
}
