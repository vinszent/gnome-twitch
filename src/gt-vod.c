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

#include "gt-vod.h"
#include "gt-app.h"
#include "gt-http.h"
#include "utils.h"
#include <gdk-pixbuf/gdk-pixbuf.h>

#define TAG "GtVOD"
#include "gnome-twitch/gt-log.h"

#define PREVIEW_WIDTH 320
#define PREVIEW_HEIGHT 180

typedef struct
{
    GtVODData* data;

    gchar* preview_filepath;

    gboolean updating;

    GdkPixbuf* preview;
    GCancellable* cancel;
} GtVODPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtVOD, gt_vod, G_TYPE_OBJECT);

enum
{
    PROP_0,
    PROP_CREATED_AT,
    PROP_PUBLISHED_AT,
    PROP_TITLE,
    PROP_DESCRIPTION,
    PROP_GAME,
    PROP_LANGUAGE,
    PROP_LENGTH,
    PROP_PREVIEW,
    PROP_VIEWS,
    PROP_URL,
    PROP_TAG_LIST,
    PROP_UPDATING,
    NUM_PROPS,
};

static GParamSpec* props[NUM_PROPS];

static void
handle_preview_download_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    RETURN_IF_FAIL(G_IS_ASYNC_RESULT(res));
    RETURN_IF_FAIL(udata != NULL);

    g_autoptr(GWeakRef) ref = udata;
    g_autoptr(GtVOD) self = g_weak_ref_get(ref);

    if (!self)
    {
        TRACE("Unreffed while wating");
        return;
    }

    GtVODPrivate* priv = gt_vod_get_instance_private(self);
    g_autoptr(GError) err = NULL;

    priv->preview = gdk_pixbuf_new_from_stream_finish(res, &err);

    RETURN_IF_ERROR(err); /* FIXME: Handle error */

    if (g_object_steal_data(G_OBJECT(self), "save-preview"))
    {
        gdk_pixbuf_save(priv->preview, priv->preview_filepath, "jpeg",
            &err, "quality", "100", NULL);

        /* NOTE: Don't need to show the user this error as it just
         * means we couldn't cache the image */
        RETURN_IF_ERROR(err);
    }

    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_PREVIEW]);

    priv->updating = FALSE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_UPDATING]);
}

static void
handle_preview_response_cb(GtHTTP* http,
    gpointer res, GError* error, gpointer udata)
{
    RETURN_IF_FAIL(GT_IS_HTTP(http));
    RETURN_IF_FAIL(udata != NULL);

    g_autoptr(GWeakRef) ref = udata;
    g_autoptr(GtVOD) self = g_weak_ref_get(ref);

    if (!self) {TRACE("Unreffed while waiting"); return;}

    GtVODPrivate* priv = gt_vod_get_instance_private(self);

    RETURN_IF_ERROR(error); /* FIXME: Handle error */

    RETURN_IF_FAIL(G_IS_INPUT_STREAM(res));

    gdk_pixbuf_new_from_stream_at_scale_async(res, PREVIEW_WIDTH, PREVIEW_HEIGHT,
        FALSE, priv->cancel, handle_preview_download_cb, g_steal_pointer(&ref));
}
static void
update_preview(GtVOD* self)
{
    g_assert(GT_IS_VOD(self));

    GtVODPrivate* priv = gt_vod_get_instance_private(self);

    gt_http_get_with_category(main_app->http, priv->data->preview.large, "gt-vod", DEFAULT_TWITCH_HEADERS,
        priv->cancel, G_CALLBACK(handle_preview_response_cb), utils_weak_ref_new(self), GT_HTTP_FLAG_RETURN_STREAM | GT_HTTP_FLAG_CACHE_RESPONSE);
}

static void
update_from_data(GtVOD* self, GtVODData* data)
{
    g_assert(GT_IS_VOD(self));
    g_assert(data != NULL);

    GtVODPrivate* priv = gt_vod_get_instance_private(self);

    RETURN_IF_FAIL(priv->data == NULL);

    priv->updating = TRUE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_UPDATING]);

    utils_refresh_cancellable(&priv->cancel);

    priv->data = data;
    priv->preview_filepath = g_build_filename(g_get_user_cache_dir(),
        "gnome-twitch", "vods", data->id, NULL);

    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_CREATED_AT]);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_PUBLISHED_AT]);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_TITLE]);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_DESCRIPTION]);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_GAME]);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_LANGUAGE]);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_LENGTH]);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_VIEWS]);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_URL]);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_TAG_LIST]);

    update_preview(self);
}

static void
get_property(GObject* obj,
    guint prop, GValue* val, GParamSpec* pspec)
{
    GtVOD* self = GT_VOD(obj);
    GtVODPrivate* priv = gt_vod_get_instance_private(self);

    switch (prop)
    {
        case PROP_CREATED_AT:
            g_value_set_boxed(val, priv->data->created_at);
            break;
        case PROP_PUBLISHED_AT:
            g_value_set_boxed(val, priv->data->published_at);
            break;
        case PROP_TITLE:
            g_value_set_string(val, priv->data->title);
            break;
        case PROP_GAME:
            g_value_set_string(val, priv->data->game);
            break;
        case PROP_PREVIEW:
            g_value_set_object(val, priv->preview);
            break;
        case PROP_UPDATING:
            g_value_set_boolean(val, priv->updating);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_vod_class_init(GtVODClass* klass)
{
    G_OBJECT_CLASS(klass)->get_property = get_property;

    props[PROP_CREATED_AT] = g_param_spec_boxed("created-at", "Created at", "Date and time when VOD was created",
        G_TYPE_DATE_TIME, G_PARAM_READABLE);

    props[PROP_PUBLISHED_AT] = g_param_spec_boxed("published-at", "Published at", "Date and time when VOD was published",
        G_TYPE_DATE_TIME, G_PARAM_READABLE);

    props[PROP_TITLE] = g_param_spec_string("title", "Title", "Title of the VOD",
        NULL, G_PARAM_READABLE);

    props[PROP_DESCRIPTION] = g_param_spec_string("description", "Description", "Description of the VOD",
        NULL, G_PARAM_READABLE);

    props[PROP_LANGUAGE] = g_param_spec_string("language", "Language", "Language of the VOD",
        NULL, G_PARAM_READABLE);

    props[PROP_GAME] = g_param_spec_string("game", "Game", "Game being played in the VOD",
        NULL, G_PARAM_READABLE);

    props[PROP_URL] = g_param_spec_string("url", "URL", "URL to the VOD",
        NULL, G_PARAM_READABLE);

    props[PROP_TAG_LIST] = g_param_spec_string("tag-list", "Tag list", "Tag list of the VOD",
        NULL, G_PARAM_READABLE);

    props[PROP_LENGTH] = g_param_spec_int64("length", "Length", "Length of the VOD in seconds",
        0, G_MAXINT64, 0, G_PARAM_READABLE);

    props[PROP_VIEWS] = g_param_spec_int64("views", "Views", "Number of views the VOD has",
        0, G_MAXINT64, 0, G_PARAM_READABLE);

    props[PROP_PREVIEW] = g_param_spec_object("preview", "Preview", "Preview pixbuf of the VOD",
        GDK_TYPE_PIXBUF, G_PARAM_READABLE);

    props[PROP_UPDATING] = g_param_spec_boolean("updating", "Updating", "Whether updating",
        FALSE, G_PARAM_READABLE);

    g_object_class_install_properties(G_OBJECT_CLASS(klass), NUM_PROPS, props);
}

static void
gt_vod_init(GtVOD* self)
{
    g_assert(GT_IS_VOD(self));

    GtVODPrivate* priv = gt_vod_get_instance_private(self);

    priv->data = NULL;
}

GtVOD*
gt_vod_new(GtVODData* data)
{
    RETURN_VAL_IF_FAIL(data != NULL, NULL);

    GtVOD* self = g_object_new(GT_TYPE_VOD, NULL);

    update_from_data(self, data);

    return self;
}

const gchar*
gt_vod_get_id(GtVOD* self)
{
    RETURN_VAL_IF_FAIL(GT_IS_VOD(self), NULL);

    GtVODPrivate* priv = gt_vod_get_instance_private(self);

    return priv->data->id;
}

gboolean
gt_vod_get_updating(GtVOD* self)
{
    RETURN_VAL_IF_FAIL(GT_IS_VOD(self), FALSE);

    GtVODPrivate* priv = gt_vod_get_instance_private(self);

    return priv->updating;
}

GtVODData*
gt_vod_data_new()
{
    return g_slice_new(GtVODData);
}

void
gt_vod_data_free(GtVODData* data)
{
    g_free(data->id);
    g_free(data->broadcast_id);
    g_free(data->description);
    g_free(data->game);
    g_free(data->language);
    /* NOTE: Uncomment when needed */
    /* gt_channel_data_free(data->channel); */
    g_free(data->preview.large);
    g_free(data->preview.medium);
    g_free(data->preview.small);
    g_free(data->preview.template);
    g_free(data->title);
    g_free(data->url);
    g_free(data->tag_list);
    g_date_time_unref(data->created_at);
    g_date_time_unref(data->published_at);

    g_slice_free(GtVODData, data);
}
