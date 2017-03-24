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

#include "gt-game-container-view.h"
#include "gt-top-game-container.h"
#include "gt-search-game-container.h"
#include "gt-game-channel-container.h"
#include "utils.h"

typedef struct
{
    GtTopGameContainer* top_container;
    GtSearchGameContainer* search_container;
    GtGameChannelContainer* game_container;

    GtkWidget* container_stack;
} GtGameContainerViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtGameContainerView, gt_game_container_view, GT_TYPE_CONTAINER_VIEW);

static void
search_active_cb(GObject* obj,
    GParamSpec* pspec, gpointer udata)
{
    g_assert(GT_IS_GAME_CONTAINER_VIEW(udata));

    GtGameContainerView* self = GT_GAME_CONTAINER_VIEW(udata);
    GtGameContainerViewPrivate* priv = gt_game_container_view_get_instance_private(self);
    GtkWidget* container_stack = gt_container_view_get_container_stack(
        GT_CONTAINER_VIEW(self));

    g_assert(GTK_IS_STACK(container_stack));

    if (gt_container_view_get_search_active(GT_CONTAINER_VIEW(self)))
        gtk_stack_set_visible_child(GTK_STACK(container_stack), GTK_WIDGET(priv->search_container));
    else
    {
        g_object_set(self, "show-back-button", FALSE, NULL);

        gtk_stack_set_visible_child(GTK_STACK(container_stack), GTK_WIDGET(priv->top_container));

        gtk_widget_set_sensitive(gt_container_view_get_search_bar(GT_CONTAINER_VIEW(self)), TRUE);
    }
}

static void
go_back(GtContainerView* view)
{
    g_assert(GT_IS_GAME_CONTAINER_VIEW(view));

    GtGameContainerView* self = GT_GAME_CONTAINER_VIEW(view);
    GtGameContainerViewPrivate* priv = gt_game_container_view_get_instance_private(self);

    GtkWidget* container_stack = gt_container_view_get_container_stack(
        GT_CONTAINER_VIEW(self));

    g_assert(GTK_IS_STACK(container_stack));

    if (gt_container_view_get_search_active(GT_CONTAINER_VIEW(self)))
    {
        GtkWidget* search_bar = gt_container_view_get_search_bar(GT_CONTAINER_VIEW(self));

        gtk_widget_set_sensitive(search_bar, TRUE);

        gtk_stack_set_visible_child(GTK_STACK(container_stack), GTK_WIDGET(priv->search_container));
    }
    else
        gtk_stack_set_visible_child(GTK_STACK(container_stack), GTK_WIDGET(priv->top_container));

    g_object_set(self, "show-back-button", FALSE, NULL);

}

static void
refresh(GtContainerView* view)
{
    g_assert(GT_IS_GAME_CONTAINER_VIEW(view));

    GtGameContainerView* self = GT_GAME_CONTAINER_VIEW(view);
    GtGameContainerViewPrivate* priv = gt_game_container_view_get_instance_private(self);

    GtkWidget* container = gtk_stack_get_visible_child(
        GTK_STACK(gt_container_view_get_container_stack(view)));

    g_assert(GT_IS_ITEM_CONTAINER(container));

    gt_item_container_refresh(GT_ITEM_CONTAINER(container));
}

static void
constructed(GObject* obj)
{
    GtGameContainerView* self = GT_GAME_CONTAINER_VIEW(obj);
    GtGameContainerViewPrivate* priv = gt_game_container_view_get_instance_private(self);
    GtkWidget* search_entry = gt_container_view_get_search_entry(GT_CONTAINER_VIEW(self));

    G_OBJECT_CLASS(gt_game_container_view_parent_class)->constructed(obj);

    g_assert(GTK_IS_SEARCH_ENTRY(search_entry));

    gt_container_view_add_container(GT_CONTAINER_VIEW(self),
        GT_ITEM_CONTAINER(priv->top_container));

    gt_container_view_add_container(GT_CONTAINER_VIEW(self),
        GT_ITEM_CONTAINER(priv->search_container));

    gt_container_view_add_container(GT_CONTAINER_VIEW(self),
        GT_ITEM_CONTAINER(priv->game_container));

    g_signal_connect(self, "notify::search-active", G_CALLBACK(search_active_cb), self);

    g_object_bind_property(search_entry, "text", priv->search_container, "query", G_BINDING_DEFAULT);
}

static void
gt_game_container_view_class_init(GtGameContainerViewClass* klass)
{
    G_OBJECT_CLASS(klass)->constructed = constructed;

    GT_CONTAINER_VIEW_CLASS(klass)->go_back = go_back;
    GT_CONTAINER_VIEW_CLASS(klass)->refresh = refresh;
}

static void
gt_game_container_view_init(GtGameContainerView* self)
{
    GtGameContainerViewPrivate* priv = gt_game_container_view_get_instance_private(self);

    priv->top_container = gt_top_game_container_new();
    priv->search_container = gt_search_game_container_new();
    priv->game_container = gt_game_channel_container_new();
    priv->container_stack = gt_container_view_get_container_stack(GT_CONTAINER_VIEW(self));
}

void
gt_game_container_view_open_game(GtGameContainerView* self, GtGame* game)
{
    g_assert(GT_IS_GAME_CONTAINER_VIEW(self));
    g_assert(GT_IS_GAME(game));

    GtGameContainerViewPrivate* priv = gt_game_container_view_get_instance_private(self);

    if (gt_container_view_get_search_active(GT_CONTAINER_VIEW(self)))
    {
        GtkWidget* search_bar = gt_container_view_get_search_bar(GT_CONTAINER_VIEW(self));

        gtk_widget_set_sensitive(search_bar, FALSE);
    }

    g_object_set(priv->game_container, "game", gt_game_get_name(game), NULL);

    g_object_set(self, "show-back-button", TRUE, NULL);

    gtk_stack_set_visible_child(GTK_STACK(priv->container_stack),
        GTK_WIDGET(priv->game_container));
}
