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

#include <glib/gi18n.h>
#include "gt-top-channel-container.h"
#include "gt-twitch.h"
#include "gt-app.h"
#include "gt-win.h"
#include "gt-channels-container-child.h"
#include "gt-channel.h"
#include "utils.h"

#define TAG "GtTopChannelContainer"
#include "gnome-twitch/gt-log.h"

#define CHILD_WIDTH 350
#define CHILD_HEIGHT 230
#define APPEND_EXTRA TRUE

typedef struct
{
    JsonParser* json_parser;
    GCancellable* cancel;
} GtTopChannelContainerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtTopChannelContainer, gt_top_channel_container, GT_TYPE_ITEM_CONTAINER);

static void
get_properties(GtItemContainer* self,
    gint* child_width, gint* child_height, gboolean* append_extra,
    gchar** empty_label_text, gchar** empty_sub_label_text, gchar** empty_image_name,
    gchar** error_label_text, gchar** fetching_label_text)
{
    *child_width = CHILD_WIDTH;
    *child_height = CHILD_HEIGHT;
    *append_extra = APPEND_EXTRA;
    *empty_label_text = g_strdup(_("No channels found"));
    *empty_sub_label_text = g_strdup(_("Probably an error occurred, try refreshing"));
    *error_label_text = g_strdup(_("An error occurred when fetching the channels"));
    *empty_image_name = g_strdup("view-refresh-symbolic");
    *fetching_label_text = g_strdup(_("Fetching channels"));
}

static void
process_json_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    DEBUG("Processing json");

    g_autoptr(GtTopChannelContainer) self = udata;
    GtTopChannelContainerPrivate* priv = gt_top_channel_container_get_instance_private(self);
    g_autoptr(JsonReader) reader = NULL;
    g_autoptr(GtChannelList) items = NULL;
    g_autoptr(GError) err = NULL;

    json_parser_load_from_stream_finish(priv->json_parser, res, &err);

    /* TODO: Handle error */
    if (err) RETURN_IF_REACHED();

    reader = json_reader_new(json_parser_get_root(priv->json_parser));

    /* TODO: Handle error */
    if (!json_reader_read_member(reader, "streams")) RETURN_IF_REACHED();

    for (gint i = 0; i < json_reader_count_elements(reader); i++)
    {
        g_autoptr(GError) err = NULL;

        /* TODO: Handle error */
        if (!json_reader_read_element(reader, i)) RETURN_IF_REACHED();

        items = g_list_append(items,
            gt_channel_new(utils_parse_stream_from_json(reader, &err)));

        json_reader_end_element(reader);
    }

    json_reader_end_member(reader);

    gt_item_container_append_items(GT_ITEM_CONTAINER(self), g_steal_pointer(&items));
    gt_item_container_set_fetching_items(GT_ITEM_CONTAINER(self), FALSE);
}

static void
handle_response_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    DEBUG("Received response");

    g_autoptr(GtTopChannelContainer) self = udata;
    GtTopChannelContainerPrivate* priv = gt_top_channel_container_get_instance_private(self);
    g_autoptr(GInputStream) istream = NULL;
    g_autoptr(GError) err = NULL;

    istream = soup_session_send_finish(SOUP_SESSION(source), res, &err);

    if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        DEBUG("Cancelled");
        return;
    }
    else if (err)
        RETURN_IF_REACHED(); /* FIXME: Handle error */

    json_parser_load_from_stream_async(priv->json_parser, istream,
        priv->cancel, process_json_cb, g_object_ref(self));
}

static void
request_extra_items(GtItemContainer* item_container,
    gint amount, gint offset)
{
    RETURN_IF_FAIL(GT_IS_TOP_CHANNEL_CONTAINER(item_container));
    RETURN_IF_FAIL(amount > 0);
    RETURN_IF_FAIL(amount <= 100);
    RETURN_IF_FAIL(offset >= 0);

    INFO("Requesting '%d' items at offset '%d'", amount, offset);

    GtTopChannelContainer* self = GT_TOP_CHANNEL_CONTAINER(item_container);
    GtTopChannelContainerPrivate* priv = gt_top_channel_container_get_instance_private(self);
    g_autoptr(SoupMessage) msg = NULL;

    utils_refresh_cancellable(&priv->cancel);

    msg = utils_create_twitch_request_v("https://api.twitch.tv/kraken/streams?limit=%d&offset=%d&language=%s",
        amount, offset, gt_app_get_language_filter(main_app));

    soup_session_send_async(main_app->soup, msg, priv->cancel, handle_response_cb, g_object_ref(self));
}

static GtkWidget*
create_child(GtItemContainer* item_container, gpointer data)
{
    g_assert(GT_IS_TOP_CHANNEL_CONTAINER(item_container));
    g_assert(GT_IS_CHANNEL(data));

    return GTK_WIDGET(gt_channels_container_child_new(GT_CHANNEL(data)));
}

static void
activate_child(GtItemContainer* item_container,
    gpointer child)
{
    gt_win_open_channel(GT_WIN_ACTIVE,
        GT_CHANNELS_CONTAINER_CHILD(child)->channel);
}

static void
gt_top_channel_container_class_init(GtTopChannelContainerClass* klass)
{
    GT_ITEM_CONTAINER_CLASS(klass)->create_child = create_child;
    GT_ITEM_CONTAINER_CLASS(klass)->get_properties = get_properties;
    GT_ITEM_CONTAINER_CLASS(klass)->activate_child = activate_child;
    GT_ITEM_CONTAINER_CLASS(klass)->request_extra_items = request_extra_items;
}

static void
gt_top_channel_container_init(GtTopChannelContainer* self)
{
    GtTopChannelContainerPrivate* priv = gt_top_channel_container_get_instance_private(self);

    priv->json_parser = json_parser_new();
}

GtTopChannelContainer*
gt_top_channel_container_new()
{
    return g_object_new(GT_TYPE_TOP_CHANNEL_CONTAINER,
        NULL);
}
