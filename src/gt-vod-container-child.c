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

#include "gt-vod-container-child.h"
#include "utils.h"

#define TAG "GtVODContainerChild"
#include "gnome-twitch/gt-log.h"

typedef struct
{
    GtkWidget* preview_overlay;
    GtkWidget* preview_image;
    GtkWidget* action_revealer;
    GtkWidget* title_label;
} GtVODContainerChildPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtVODContainerChild, gt_vod_container_child, GTK_TYPE_FLOW_BOX_CHILD);

enum
{
    PROP_0,
    PROP_VOD,
    NUM_PROPS,
};

static GParamSpec* props[NUM_PROPS];

static gboolean
preview_mouse_enter_cb(GtkWidget* source,
    GdkEvent* evt, gpointer udata)
{
    GtVODContainerChild* self = GT_VOD_CONTAINER_CHILD(udata);
    GtVODContainerChildPrivate* priv = gt_vod_container_child_get_instance_private(self);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->action_revealer), TRUE);

    return GDK_EVENT_PROPAGATE;
}

static gboolean
preview_mouse_leave_cb(GtkWidget* source,
    GdkEvent* evt, gpointer udata)
{
    GtVODContainerChild* self = GT_VOD_CONTAINER_CHILD(udata);
    GtVODContainerChildPrivate* priv = gt_vod_container_child_get_instance_private(self);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->action_revealer), FALSE);

    return GDK_EVENT_PROPAGATE;
}

static void
get_property(GObject* obj, guint prop,
    GValue* val, GParamSpec* pspec)
{
    GtVODContainerChild* self = GT_VOD_CONTAINER_CHILD(obj);

    switch (prop)
    {
        case PROP_VOD:
            g_value_set_object(val, self->vod);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
set_property(GObject* obj, guint prop,
    const GValue* val, GParamSpec* pspec)
{
    GtVODContainerChild* self = GT_VOD_CONTAINER_CHILD(obj);

    switch (prop)
    {
        case PROP_VOD:
            self->vod = utils_value_ref_sink_object(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
constructed(GObject* obj)
{
    g_assert(GT_IS_VOD_CONTAINER_CHILD(obj));

    GtVODContainerChild* self = GT_VOD_CONTAINER_CHILD(obj);
    GtVODContainerChildPrivate* priv = gt_vod_container_child_get_instance_private(self);

    g_object_bind_property(self->vod, "preview", priv->preview_image, "pixbuf",
        G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(self->vod, "title", priv->title_label, "label",
        G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

    gtk_widget_set_events(priv->preview_overlay, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);

    g_signal_connect_object(priv->preview_overlay, "enter-notify-event",
        G_CALLBACK(preview_mouse_enter_cb), self, 0);
    g_signal_connect_object(priv->preview_overlay, "leave-notify-event",
        G_CALLBACK(preview_mouse_leave_cb), self, 0);

    if (G_OBJECT_CLASS(gt_vod_container_child_parent_class)->constructed)
        G_OBJECT_CLASS(gt_vod_container_child_parent_class)->constructed(obj);
}

static void
gt_vod_container_child_class_init(GtVODContainerChildClass* klass)
{
    G_OBJECT_CLASS(klass)->get_property = get_property;
    G_OBJECT_CLASS(klass)->set_property = set_property;
    G_OBJECT_CLASS(klass)->constructed = constructed;

    props[PROP_VOD] = g_param_spec_object("vod", "VOD", "VOD",
        GT_TYPE_VOD, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties(G_OBJECT_CLASS(klass), NUM_PROPS, props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
        "/com/vinszent/GnomeTwitch/ui/gt-vod-container-child.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtVODContainerChild, action_revealer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtVODContainerChild, preview_overlay);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtVODContainerChild, preview_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtVODContainerChild, title_label);
}

static void
gt_vod_container_child_init(GtVODContainerChild* self)
{
    g_assert(GT_IS_VOD_CONTAINER_CHILD(self));

    gtk_widget_init_template(GTK_WIDGET(self));
}

GtVODContainerChild*
gt_vod_container_child_new(GtVOD* vod)
{
    return g_object_new(GT_TYPE_VOD_CONTAINER_CHILD,
        "vod", vod, NULL);
}
