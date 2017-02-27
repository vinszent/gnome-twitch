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
#include "gt-search-game-container.h"
#include "gt-games-container-child.h"
#include "gt-game.h"
#include "gt-twitch.h"
#include "gt-app.h"
#include "gt-win.h"
#include "gt-game-container-view.h"
#include "utils.h"

#define CHILD_WIDTH 210
#define CHILD_HEIGHT 320
#define APPEND_EXTRA TRUE
#define SEARCH_DELAY G_TIME_SPAN_MILLISECOND * 500 //ms

typedef struct
{
    gchar* query;
    GMutex mutex;
    GCond cond;
} GtSearchGameContainerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtSearchGameContainer, gt_search_game_container, GT_TYPE_ITEM_CONTAINER);

enum
{
    PROP_0,
    PROP_QUERY,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

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
    *empty_sub_label_text = g_strdup(_("Try a different search"));
    *error_label_text = g_strdup(_("An error occurred when fetching the games"));
    *empty_image_name = g_strdup("edit-find-symbolic");
    *fetching_label_text = g_strdup(_("Fetching games"));
}

static void
cancelled_cb(GCancellable* cancel,
    gpointer udata)
{
    GtSearchGameContainer* self = GT_SEARCH_GAME_CONTAINER(udata);
    GtSearchGameContainerPrivate* priv = gt_search_game_container_get_instance_private(self);

    g_assert(GT_IS_SEARCH_GAME_CONTAINER(self));
    g_assert(G_IS_CANCELLABLE(cancel));

    g_cond_signal(&priv->cond);
}

static void
fetch_items(GTask* task, gpointer source, gpointer task_data, GCancellable* cancel)
{
    g_assert(GT_IS_SEARCH_GAME_CONTAINER(source));

    GtSearchGameContainer* self = GT_SEARCH_GAME_CONTAINER(source);
    GtSearchGameContainerPrivate* priv = gt_search_game_container_get_instance_private(self);

    g_assert(G_IS_CANCELLABLE(cancel));
    g_assert(G_IS_TASK(task));

    if (g_task_return_error_if_cancelled(task))
        return;

    if (utils_str_empty(priv->query))
    {
        g_task_return_pointer(task, NULL, NULL);
        return;
    }

    g_assert_nonnull(task_data);

    FetchItemsData* data = task_data;

    g_mutex_lock(&priv->mutex);

    gint64 end_time = g_get_monotonic_time() + SEARCH_DELAY;

    g_cancellable_connect(cancel, G_CALLBACK(cancelled_cb), self, NULL);

    while (!g_cancellable_is_cancelled(cancel))
    {
        if (!g_cond_wait_until(&priv->cond, &priv->mutex, end_time))
        {
            /* We weren't cancelled */

            g_assert(!utils_str_empty(priv->query));

            GError* err = NULL;

            GList* items = gt_twitch_search_games(main_app->twitch,
                priv->query, data->amount, data->offset, &err);

            if (err)
                g_task_return_error(task, err);
            else
                g_task_return_pointer(task, items, (GDestroyNotify) gt_game_list_free);

            g_mutex_unlock(&priv->mutex);

            return;
        }
    }

    /* We were cancelled */
    g_assert(g_task_return_error_if_cancelled(task));

    g_mutex_unlock(&priv->mutex);
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
    g_assert(GT_IS_SEARCH_GAME_CONTAINER(item_container));
    g_assert(GT_IS_GAMES_CONTAINER_CHILD(child));

    GtkWidget* parent = gtk_widget_get_ancestor(GTK_WIDGET(item_container),
        GT_TYPE_GAME_CONTAINER_VIEW);

    g_assert_nonnull(parent);
    g_assert(GT_IS_GAME_CONTAINER_VIEW(parent));

    gt_game_container_view_open_game(GT_GAME_CONTAINER_VIEW(parent),
        gt_games_container_child_get_game(GT_GAMES_CONTAINER_CHILD(child)));
}

static void
get_property(GObject* obj,
    guint prop,
    GValue* val,
    GParamSpec* pspec)
{
    GtSearchGameContainer* self = GT_SEARCH_GAME_CONTAINER(obj);
    GtSearchGameContainerPrivate* priv = gt_search_game_container_get_instance_private(self);

    switch (prop)
    {
        case PROP_QUERY:
            g_value_set_string(val, priv->query);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
set_property(GObject* obj,
    guint prop,
    const GValue* val,
    GParamSpec* pspec)
{
    GtSearchGameContainer* self = GT_SEARCH_GAME_CONTAINER(obj);
    GtSearchGameContainerPrivate* priv = gt_search_game_container_get_instance_private(self);

    switch (prop)
    {
        case PROP_QUERY:
            g_clear_pointer(&priv->query, g_free);
            priv->query = g_value_dup_string(val);

            gt_item_container_refresh(GT_ITEM_CONTAINER(self));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_search_game_container_class_init(GtSearchGameContainerClass* klass)
{
    G_OBJECT_CLASS(klass)->get_property = get_property;
    G_OBJECT_CLASS(klass)->set_property = set_property;

    GT_ITEM_CONTAINER_CLASS(klass)->create_child = create_child;
    GT_ITEM_CONTAINER_CLASS(klass)->get_properties = get_properties;
    GT_ITEM_CONTAINER_CLASS(klass)->fetch_items = fetch_items;
    GT_ITEM_CONTAINER_CLASS(klass)->activate_child = activate_child;

    props[PROP_QUERY] = g_param_spec_string("query", "Query", "Current query", NULL, G_PARAM_READWRITE);

    g_object_class_install_properties(G_OBJECT_CLASS(klass), NUM_PROPS, props);
}

static void
gt_search_game_container_init(GtSearchGameContainer* self)
{
    GtSearchGameContainerPrivate* priv = gt_search_game_container_get_instance_private(self);

    priv->query = NULL;
    g_cond_init(&priv->cond);
    g_mutex_init(&priv->mutex);
}

GtSearchGameContainer*
gt_search_game_container_new()
{
    return g_object_new(GT_TYPE_SEARCH_GAME_CONTAINER,
        NULL);
}
