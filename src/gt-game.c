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
#include "gt-resource-downloader.h"
#include "utils.h"
#include <glib/gi18n.h>

#define TAG "GtGame"
#include "gnome-twitch/gt-log.h"

typedef struct
{
    GtGameData* data;

    gboolean updating;

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

static GtResourceDownloader* res_downloader;
static GThreadPool* update_preview_pool;

static GParamSpec* props[NUM_PROPS];

static gboolean
notify_preview_cb(gpointer udata)
{
    RETURN_VAL_IF_FAIL(GT_IS_GAME(self), G_SOURCE_REMOVE);

    /* NOTE: Callback to async func so we need to unref ourselves */
    g_autoptr(GtGame) self = GT_GAME(udata);
    GtGamePrivate* priv = gt_game_get_instance_private(self);

    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_PREVIEW]);

    priv->updating = FALSE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_UPDATING]);

    return G_SOURCE_REMOVE;
}

static void
update_preview(GtGame* self)
{
    RETURN_IF_FAIL(GT_IS_GAME(self));

    GtGamePrivate* priv = gt_game_get_instance_private(self);

    utils_refresh_cancellable(&priv->cancel);

    /* FIXME: Handle error below */
    priv->preview = gt_resource_downloader_download_image(res_downloader,
        priv->data->preview_url, priv->data->id, TRUE, NULL);

    utils_pixbuf_scale_simple(&priv->preview,
        200, 270, GDK_INTERP_BILINEAR);

    g_idle_add(notify_preview_cb, self);
}

static void
update_from_data(GtGame* self, GtGameData* data)
{
    RETURN_IF_FAIL(GT_IS_GAME(self));
    RETURN_IF_FAIL(data != NULL);

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

            WARNING("Unable to update game with id '%s' and name '%s' because: "
                "New data with id '%s' does not match the current one",
                old_data->id, old_data->name, data->id);

            /* FIXME: Put GtGame in a error state like GtChannel */
            if (GT_IS_WIN(win))
            {
                gt_win_show_error_message(win, "Unable to update game '%s'",
                    "Unable to update game with id '%s' and name '%s' because: "
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

    /* FIXME: Handle error below */
    g_thread_pool_push(update_preview_pool, g_object_ref(self), NULL);
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

    g_autofree gchar* filepath = g_build_filename(g_get_user_cache_dir(), "gnome-twitch", "games", NULL);

    update_preview_pool = g_thread_pool_new((GFunc) update_preview, NULL, g_get_num_processors(), FALSE, NULL);
    res_downloader = gt_resource_downloader_new(filepath);
}

static void
gt_game_init(GtGame* self)
{
    g_assert(GT_IS_GAME(self));

    GtGamePrivate* priv = gt_game_get_instance_private(self);

    priv->updating = FALSE;
    priv->preview = NULL;
    priv->logo = NULL;
}

GtGame*
gt_game_new(GtGameData* data)
{
    RETURN_VAL_IF_FAIL(GT_IS_GAME(self), NULL);

    GtGame* game = g_object_new(GT_TYPE_GAME, NULL);

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
    RETURN_VAL_IF_FAIL(GT_IS_GAME(self), NULL);

    GtGamePrivate* priv = gt_game_get_instance_private(self);

    return priv->data->name;
}

gboolean
gt_game_get_updating(GtGame* self)
{
    RETURN_VAL_IF_FAIL(GT_IS_GAME(self), FALSE);

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
