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

#define CHILD_WIDTH 210
#define CHILD_HEIGHT 320
#define APPEND_EXTRA TRUE

typedef struct
{
    gpointer tmp;
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
fetch_items(GTask* task,
    gpointer source, gpointer task_data,
    GCancellable* cancel)
{
    g_assert(G_IS_TASK(task));

    if (g_task_return_error_if_cancelled(task))
        return;

    FetchItemsData* data = task_data;

    g_assert_nonnull(data);

    GError* err = NULL;

    GList* items = gt_twitch_top_games(main_app->twitch,
        data->amount, data->offset, &err);

    if (err)
        g_task_return_error(task, err);
    else
        g_task_return_pointer(task, items, (GDestroyNotify) gt_game_list_free);
}

static GtkWidget*
create_child(gpointer data)
{
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
    GT_ITEM_CONTAINER_CLASS(klass)->fetch_items = fetch_items;
    GT_ITEM_CONTAINER_CLASS(klass)->activate_child = activate_child;
}

static void
gt_top_game_container_init(GtTopGameContainer* self)
{

}

GtTopGameContainer*
gt_top_game_container_new()
{
    return g_object_new(GT_TYPE_TOP_GAME_CONTAINER,
        NULL);
}
