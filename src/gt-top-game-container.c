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
#include "gt-top-game-container.h"
#include "gt-game.h"
#include "gt-games-container-child.h"
#include "gt-app.h"
#include "gt-twitch.h"
#include "gt-game-container-view.h"
#include "utils.h"

#define TAG "GtTopGameContainer"
#include "gnome-twitch/gt-log.h"

#define CHILD_WIDTH 210
#define CHILD_HEIGHT 320
#define APPEND_EXTRA TRUE

typedef struct
{
    JsonParser* json_parser;
    GCancellable* cancel;
} GtTopGameContainerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtTopGameContainer, gt_top_game_container, GT_TYPE_ITEM_CONTAINER);

static void
get_properties(GtItemContainer* self,
    gint* child_width, gint* child_height, gboolean* append_extra,
    gchar** empty_label_text, gchar** empty_sub_label_text, gchar** empty_image_name,
    gchar** error_label_text, gchar** fetching_label_text)
{
    *child_width = CHILD_WIDTH;
    *child_height = CHILD_HEIGHT;
    *append_extra = APPEND_EXTRA;
    *empty_label_text = g_strdup(_("No games found"));
    *empty_sub_label_text = g_strdup(_("Probably an error occurred, try refreshing"));
    *error_label_text = g_strdup(_("An error occurred when fetching the games"));
    *empty_image_name = g_strdup("view-refresh-symbolic");
    *fetching_label_text = g_strdup(_("Fetching games"));
}

static void
process_json_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    RETURN_IF_FAIL(JSON_IS_PARSER(source));
    RETURN_IF_FAIL(G_IS_ASYNC_RESULT(res));
    RETURN_IF_FAIL(udata != NULL);

    g_autoptr(GWeakRef) ref = udata;
    g_autoptr(GtTopGameContainer) self = g_weak_ref_get(ref);

    if (!self)
    {
        TRACE("Not processing json because we were unreffed while waiting");
        return;
    }

    GtTopGameContainerPrivate* priv = gt_top_game_container_get_instance_private(self);
    g_autoptr(JsonReader) reader = NULL;
    g_autoptr(GtGameList) items = NULL;
    g_autoptr(GError) err = NULL;

    json_parser_load_from_stream_finish(priv->json_parser, res, &err);

    if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        DEBUG("Cancelled");
        return;
    }
    else if (err)
    {
        WARNING("Unable to process JSON because: %s", err->message);
        gt_item_container_show_error(GT_ITEM_CONTAINER(self), err);
        return;
    }

    reader = json_reader_new(json_parser_get_root(priv->json_parser));

    if (!json_reader_read_member(reader, "top"))
    {
        WARNING("Unable to process JSON because: %s",
            json_reader_get_error(reader)->message);
        gt_item_container_show_error(GT_ITEM_CONTAINER(self),
            json_reader_get_error(reader));
        return;
    }

    for (gint i = 0; i < json_reader_count_elements(reader); i++)
    {
        g_autoptr(GtGameData) data = NULL;

        if (!json_reader_read_element(reader, i))
        {
            WARNING("Unable to process JSON because: %s",
                json_reader_get_error(reader)->message);
            gt_item_container_show_error(GT_ITEM_CONTAINER(self),
                json_reader_get_error(reader));
            return;
        }

        if (!json_reader_read_member(reader, "game"))
        {
            WARNING("Unable to process JSON because: %s",
                json_reader_get_error(reader)->message);
            gt_item_container_show_error(GT_ITEM_CONTAINER(self),
                json_reader_get_error(reader));
            return;
        }

        data = utils_parse_game_from_json(reader, &err);

        if (err)
        {
            WARNING("Unable to process JSON because: %s", err->message);
            gt_item_container_show_error(GT_ITEM_CONTAINER(self), err);
            return;
        }

        items = g_list_append(items, gt_game_new(g_steal_pointer(&data)));

        json_reader_end_member(reader);

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
    RETURN_IF_FAIL(SOUP_IS_SESSION(source));
    RETURN_IF_FAIL(G_IS_ASYNC_RESULT(res));
    RETURN_IF_FAIL(udata != NULL);

    g_autoptr(GWeakRef) ref = udata;
    g_autoptr(GtTopGameContainer) self = g_weak_ref_get(ref);

    if (!self)
    {
        TRACE("Not handling response because we were unreffed while waiting");
        return;
    }

    GtTopGameContainerPrivate* priv = gt_top_game_container_get_instance_private(self);
    g_autoptr(GInputStream) istream = NULL;
    g_autoptr(GError) err = NULL;

    istream = soup_session_send_finish(main_app->soup, res, &err);

    if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        DEBUG("Cancelled");
        return;
    }
    else if (err)
    {
        WARNING("Unable to handle response because: %s", err->message);
        gt_item_container_show_error(GT_ITEM_CONTAINER(self), err);
        return;
    }

    json_parser_load_from_stream_async(priv->json_parser, istream,
        priv->cancel, process_json_cb, g_steal_pointer(&ref));
}

static void
request_extra_items(GtItemContainer* item_container,
    gint amount, gint offset)
{
    RETURN_IF_FAIL(GT_IS_TOP_GAME_CONTAINER(item_container));
    RETURN_IF_FAIL(amount > 0);
    RETURN_IF_FAIL(amount <= 100);
    RETURN_IF_FAIL(offset >= 0);

    INFO("Requesting '%d' items at offset '%d'", amount, offset);

    GtTopGameContainer* self = GT_TOP_GAME_CONTAINER(item_container);
    GtTopGameContainerPrivate* priv = gt_top_game_container_get_instance_private(self);
    g_autoptr(SoupMessage) msg = NULL;

    utils_refresh_cancellable(&priv->cancel);

    msg = utils_create_twitch_request_v("https://api.twitch.tv/kraken/games/top?limit=%d&offset=%d",
        amount, offset);

    gt_app_queue_soup_message(main_app, "gt-item-container",
        msg, priv->cancel, handle_response_cb, utils_weak_ref_new(self));
}

static GtkWidget*
create_child(GtItemContainer* item_container, gpointer data)
{
    g_assert(GT_IS_TOP_GAME_CONTAINER(item_container));
    g_assert(GT_IS_GAME(data));

    return GTK_WIDGET(gt_games_container_child_new(GT_GAME(data)));
}

static void
activate_child(GtItemContainer* item_container,
    gpointer child)
{
    g_assert(GT_IS_TOP_GAME_CONTAINER(item_container));
    g_assert(GT_IS_GAMES_CONTAINER_CHILD(child));

    GtkWidget* parent = gtk_widget_get_ancestor(GTK_WIDGET(item_container),
        GT_TYPE_GAME_CONTAINER_VIEW);

    g_assert_nonnull(parent);
    g_assert(GT_IS_GAME_CONTAINER_VIEW(parent));

    gt_game_container_view_open_game(GT_GAME_CONTAINER_VIEW(parent),
        gt_games_container_child_get_game(GT_GAMES_CONTAINER_CHILD(child)));
}

static void
gt_top_game_container_class_init(GtTopGameContainerClass* klass)
{
    GT_ITEM_CONTAINER_CLASS(klass)->create_child = create_child;
    GT_ITEM_CONTAINER_CLASS(klass)->get_properties = get_properties;
    GT_ITEM_CONTAINER_CLASS(klass)->request_extra_items = request_extra_items;
    GT_ITEM_CONTAINER_CLASS(klass)->activate_child = activate_child;
}

static void
gt_top_game_container_init(GtTopGameContainer* self)
{
    GtTopGameContainerPrivate* priv = gt_top_game_container_get_instance_private(self);

    priv->json_parser = json_parser_new();
}

GtTopGameContainer*
gt_top_game_container_new()
{
    return g_object_new(GT_TYPE_TOP_GAME_CONTAINER,
        NULL);
}
