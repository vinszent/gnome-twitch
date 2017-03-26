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
#include "gt-search-channel-container.h"
#include "gt-top-channel-container.h"
#include "gt-item-container.h"
#include "gt-twitch.h"
#include "gt-app.h"
#include "gt-win.h"
#include "gt-channels-container-child.h"
#include "gt-channel.h"
#include "utils.h"

#define TAG "GtSearchChannelContainer"
#include "gnome-twitch/gt-log.h"

#define CHILD_WIDTH 350
#define CHILD_HEIGHT 230
#define APPEND_EXTRA TRUE
#define SEARCH_DELAY G_TIME_SPAN_MILLISECOND * 500 //ms

typedef struct
{
    gboolean search_offline;
    gchar* query;
    GMutex mutex;
    GCond cond;
} GtSearchChannelContainerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtSearchChannelContainer, gt_search_channel_container, GT_TYPE_ITEM_CONTAINER);

enum
{
    PROP_0,
    PROP_QUERY,
    PROP_SEARCH_OFFLINE,
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
    *empty_label_text = g_strdup(_("No channels found"));
    *empty_sub_label_text = g_strdup(_("Try a different search"));
    *error_label_text = g_strdup(_("An error occurred when fetching the channels"));
    *empty_image_name = g_strdup("edit-find-symbolic");
    *fetching_label_text = g_strdup(_("Fetching channels"));
}

static void
cancelled_cb(GCancellable* cancel,
    gpointer udata)
{
    GtSearchChannelContainer* self = GT_SEARCH_CHANNEL_CONTAINER(udata);
    GtSearchChannelContainerPrivate* priv = gt_search_channel_container_get_instance_private(self);

    g_assert(GT_IS_SEARCH_CHANNEL_CONTAINER(self));
    g_assert(G_IS_CANCELLABLE(cancel));

    g_mutex_lock(&priv->mutex);
    g_cond_signal(&priv->cond);
    g_mutex_unlock(&priv->mutex);
}

static void
fetch_items(GTask* task, gpointer source, gpointer task_data, GCancellable* cancel)
{
    GtSearchChannelContainer* self = GT_SEARCH_CHANNEL_CONTAINER(source);
    GtSearchChannelContainerPrivate* priv = gt_search_channel_container_get_instance_private(self);

    g_assert(GT_IS_SEARCH_CHANNEL_CONTAINER(self));
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

            GList* items = gt_twitch_search_channels(main_app->twitch,
                priv->query, data->amount, data->offset, priv->search_offline, &err);

            //NOTE: This is so online channels will be displayed as online
            if (priv->search_offline)
                g_list_foreach(items, (GFunc) gt_channel_update, NULL);

            if (err)
                g_task_return_error(task, err);
            else
                g_task_return_pointer(task, items, (GDestroyNotify) gt_channel_list_free);

            g_mutex_unlock(&priv->mutex);

            return;
        }
    }

    /* We were cancelled */
    g_assert(g_task_return_error_if_cancelled(task));

    g_mutex_unlock(&priv->mutex);
}

static GtkWidget*
create_child(GtItemContainer* item_container, gpointer data)
{
    g_assert(GT_IS_SEARCH_CHANNEL_CONTAINER(item_container));
    g_assert(GT_IS_CHANNEL(data));

    return GTK_WIDGET(gt_channels_container_child_new(GT_CHANNEL(data)));
}

static void
activate_child(GtItemContainer* item_container,
    gpointer child)
{
    g_assert(GT_IS_SEARCH_CHANNEL_CONTAINER(item_container));
    g_assert(GT_IS_CHANNELS_CONTAINER_CHILD(child));

    if (gt_channel_is_online(GT_CHANNELS_CONTAINER_CHILD(child)->channel))
    {
        gt_win_open_channel(GT_WIN_ACTIVE,
            GT_CHANNELS_CONTAINER_CHILD(child)->channel);
    }
}

static void
get_property(GObject* obj,
    guint prop,
    GValue* val,
    GParamSpec* pspec)
{
    GtSearchChannelContainer* self = GT_SEARCH_CHANNEL_CONTAINER(obj);
    GtSearchChannelContainerPrivate* priv = gt_search_channel_container_get_instance_private(self);

    switch (prop)
    {
        case PROP_QUERY:
            g_value_set_string(val, priv->query);
            break;
        case PROP_SEARCH_OFFLINE:
            g_value_set_boolean(val, priv->search_offline);
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
    GtSearchChannelContainer* self = GT_SEARCH_CHANNEL_CONTAINER(obj);
    GtSearchChannelContainerPrivate* priv = gt_search_channel_container_get_instance_private(self);

    switch (prop)
    {
        case PROP_QUERY:
            g_clear_pointer(&priv->query, g_free);
            priv->query = g_value_dup_string(val);

            gt_item_container_refresh(GT_ITEM_CONTAINER(self));
            break;
        case PROP_SEARCH_OFFLINE:
            priv->search_offline = g_value_get_boolean(val);

            gt_item_container_refresh(GT_ITEM_CONTAINER(self));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_search_channel_container_class_init(GtSearchChannelContainerClass* klass)
{
    G_OBJECT_CLASS(klass)->get_property = get_property;
    G_OBJECT_CLASS(klass)->set_property = set_property;

    GT_ITEM_CONTAINER_CLASS(klass)->create_child = create_child;
    GT_ITEM_CONTAINER_CLASS(klass)->get_properties = get_properties;
    GT_ITEM_CONTAINER_CLASS(klass)->fetch_items = fetch_items;
    GT_ITEM_CONTAINER_CLASS(klass)->activate_child = activate_child;

    props[PROP_QUERY] = g_param_spec_string("query", "Query", "Current query",
        NULL, G_PARAM_READWRITE);

    props[PROP_SEARCH_OFFLINE] = g_param_spec_boolean("search-offline", "Search offline", "Whether to search offline channels",
        FALSE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    g_object_class_install_properties(G_OBJECT_CLASS(klass), NUM_PROPS, props);
}

static void
gt_search_channel_container_init(GtSearchChannelContainer* self)
{
    GtSearchChannelContainerPrivate* priv = gt_search_channel_container_get_instance_private(self);

    priv->query = NULL;
    g_mutex_init(&priv->mutex);
}

GtSearchChannelContainer*
gt_search_channel_container_new()
{
    return g_object_new(GT_TYPE_SEARCH_CHANNEL_CONTAINER,
        NULL);
}

void
gt_search_channel_container_set_query(GtSearchChannelContainer* self, const gchar* query)
{
    GtSearchChannelContainerPrivate* priv = gt_search_channel_container_get_instance_private(self);

    g_assert(GT_IS_SEARCH_CHANNEL_CONTAINER(self));

    g_clear_pointer(&priv->query, g_free);
    priv->query = g_strdup(query);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_QUERY]);
}

const gchar*
gt_search_channel_container_get_query(GtSearchChannelContainer* self)
{
    GtSearchChannelContainerPrivate* priv = gt_search_channel_container_get_instance_private(self);

    g_assert(GT_IS_SEARCH_CHANNEL_CONTAINER(self));

    return priv->query;
}
