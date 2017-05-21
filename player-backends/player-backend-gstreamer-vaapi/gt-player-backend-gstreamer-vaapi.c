/*
 *  This file is part of GNOME Twitch - 'Enjoy Twitch on your GNU/Linux desktop'
 *  Copyright © 2017 Hyunjun Ko <zzoon@igalia.com>
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

#include "gt-player-backend-gstreamer-vaapi.h"
#include "gnome-twitch/gt-player-backend.h"
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#define TAG "GtPlayerBackendGstreamerVaapi"
#include "gnome-twitch/gt-log.h"

typedef struct
{
    GstElement* playbin;
    GstElement* upload;
    GstElement* video_sink;
    GstElement* video_bin;
    GstBus* bus;
    GstPad* pad;
    GstPad* ghost_pad;

    GtkWidget* widget;

    gchar* uri;

    gdouble volume;
    gboolean playing;
    gdouble buffer_fill;
} GtPlayerBackendGstreamerVaapiPrivate;

static void gt_player_backend_iface_init(GtPlayerBackendInterface* iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(GtPlayerBackendGstreamerVaapi, gt_player_backend_gstreamer_vaapi, PEAS_TYPE_EXTENSION_BASE, 0,
                               G_IMPLEMENT_INTERFACE_DYNAMIC(GT_TYPE_PLAYER_BACKEND, gt_player_backend_iface_init)
                               G_ADD_PRIVATE_DYNAMIC(GtPlayerBackendGstreamerVaapi))

enum
{
    PROP_0,
    PROP_VOLUME,
    PROP_PLAYING,
    PROP_URI,
    PROP_BUFFER_FILL,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

static GstBusSyncReply
gst_message_cb(GstBus* bus, GstMessage* msg, gpointer udata)
{
    GtPlayerBackendGstreamerVaapi* self = GT_PLAYER_BACKEND_GSTREAMER_VAAPI(udata);
    GtPlayerBackendGstreamerVaapiPrivate* priv = gt_player_backend_gstreamer_vaapi_get_instance_private(self);

    switch (GST_MESSAGE_TYPE(msg))
    {
        case GST_MESSAGE_BUFFERING:
        {
            gint perc;

            gst_message_parse_buffering(msg, &perc);

            perc < 100 ? gst_element_set_state(priv->playbin, GST_STATE_PAUSED)
                : gst_element_set_state(priv->playbin, GST_STATE_PLAYING);
            g_object_set(self, "buffer-fill", (gdouble) perc/100.0, NULL);
            break;
        }
        case GST_MESSAGE_ELEMENT:
        {
#ifdef GDK_WINDOWING_X11
            GdkWindow *window = gtk_widget_get_window(priv->widget);

            if (!gst_is_video_overlay_prepare_window_handle_message(msg))
              break;

            if (!GDK_IS_X11_DISPLAY(gtk_widget_get_display (priv->widget)))
                break;

            gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(msg)), GDK_WINDOW_XID(window));

            return GST_BUS_DROP;
#else
            break;
#endif
        }

        default:
            break;
    }

    return GST_BUS_PASS;
}

static void
play(GtPlayerBackendGstreamerVaapi* self)
{
    GtPlayerBackendGstreamerVaapiPrivate* priv = gt_player_backend_gstreamer_vaapi_get_instance_private(self);

    gst_element_set_state(priv->playbin, GST_STATE_PLAYING);
}

static void
stop(GtPlayerBackendGstreamerVaapi* self)
{
    GtPlayerBackendGstreamerVaapiPrivate* priv = gt_player_backend_gstreamer_vaapi_get_instance_private(self);

    gst_element_set_state(priv->playbin, GST_STATE_NULL);
}

static GtkWidget*
get_widget(GtPlayerBackend* backend)
{
    GtPlayerBackendGstreamerVaapi* self = GT_PLAYER_BACKEND_GSTREAMER_VAAPI(backend);
    GtPlayerBackendGstreamerVaapiPrivate* priv = gt_player_backend_gstreamer_vaapi_get_instance_private(self);

    return priv->widget;
}

static void
finalise(GObject* obj)
{
    GtPlayerBackendGstreamerVaapi* self = GT_PLAYER_BACKEND_GSTREAMER_VAAPI(obj);
    GtPlayerBackendGstreamerVaapiPrivate* priv = gt_player_backend_gstreamer_vaapi_get_instance_private(self);

    MESSAGE("Finalise");

    G_OBJECT_CLASS(gt_player_backend_gstreamer_vaapi_parent_class)->finalize(obj);

    stop(self);

    gst_object_unref(priv->playbin);
    g_clear_object(&priv->widget);

    g_free(priv->uri);
}

static void
get_property(GObject* obj,
             guint prop,
             GValue* val,
             GParamSpec* pspec)
{
    GtPlayerBackendGstreamerVaapi* self = GT_PLAYER_BACKEND_GSTREAMER_VAAPI(obj);
    GtPlayerBackendGstreamerVaapiPrivate* priv = gt_player_backend_gstreamer_vaapi_get_instance_private(self);

    switch (prop)
    {
        case PROP_VOLUME:
            g_value_set_double(val, priv->volume);
            break;
        case PROP_PLAYING:
            g_value_set_boolean(val, priv->playing);
            break;
        case PROP_URI:
            g_value_set_string(val, priv->uri);
            break;
        case PROP_BUFFER_FILL:
            g_value_set_double(val, priv->buffer_fill);
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
    GtPlayerBackendGstreamerVaapi* self = GT_PLAYER_BACKEND_GSTREAMER_VAAPI(obj);
    GtPlayerBackendGstreamerVaapiPrivate* priv = gt_player_backend_gstreamer_vaapi_get_instance_private(self);

    switch (prop)
    {
        case PROP_VOLUME:
            priv->volume = g_value_get_double(val);
            break;
        case PROP_PLAYING:
            priv->playing = g_value_get_boolean(val);
            priv->playing ? play(self) : stop(self);
            break;
        case PROP_URI:
            g_free(priv->uri);
            priv->uri = g_value_dup_string(val);
            break;
        case PROP_BUFFER_FILL:
            priv->buffer_fill = g_value_get_double(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_player_backend_iface_init(GtPlayerBackendInterface* iface)
{
    iface->get_widget = get_widget;
}

static void
gt_player_backend_gstreamer_vaapi_class_finalize(GtPlayerBackendGstreamerVaapiClass* klass)
{
    if (gst_is_initialized())
        gst_deinit();
}

static void
gt_player_backend_gstreamer_vaapi_class_init(GtPlayerBackendGstreamerVaapiClass* klass)
{
    GObjectClass* obj_class = G_OBJECT_CLASS(klass);

    obj_class->finalize = finalise;
    obj_class->get_property = get_property;
    obj_class->set_property = set_property;

    props[PROP_VOLUME] = g_param_spec_double("volume",
                                             "Volume",
                                             "Volume of player",
                                             0.0, 1.0, 0.3,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_PLAYING] = g_param_spec_boolean("playing",
                                               "Playing",
                                               "Whether playing",
                                               FALSE,
                                               G_PARAM_READABLE | G_PARAM_CONSTRUCT);
    props[PROP_URI] = g_param_spec_string("uri",
                                          "Uri",
                                          "Current uri",
                                          "",
                                          G_PARAM_READWRITE);
    props[PROP_BUFFER_FILL] = g_param_spec_double("buffer-fill",
                                                     "Buffer Fill",
                                                     "Current buffer fill",
                                                     0, 1.0, 0,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    g_object_class_override_property(obj_class, PROP_VOLUME, "volume");
    g_object_class_override_property(obj_class, PROP_PLAYING, "playing");
    g_object_class_override_property(obj_class, PROP_URI, "uri");
    g_object_class_override_property(obj_class, PROP_BUFFER_FILL, "buffer-fill");

    if (!gst_is_initialized())
        gst_init(NULL, NULL);
}

static void
gt_player_backend_gstreamer_vaapi_init(GtPlayerBackendGstreamerVaapi* self)
{
    GtPlayerBackendGstreamerVaapiPrivate* priv = gt_player_backend_gstreamer_vaapi_get_instance_private(self);

    MESSAGE("{GtPlayerBackendGstreamerVAAPI} Init");

    priv->playbin = gst_element_factory_make("playbin", NULL);
    priv->video_sink = gst_element_factory_make("vaapisink", NULL);

    priv->bus = gst_element_get_bus(priv->playbin);
    gst_bus_set_sync_handler (priv->bus, gst_message_cb, (gpointer) self, NULL);

    g_object_set(priv->playbin, "video-sink", priv->video_sink, NULL);
    priv->widget = gtk_drawing_area_new();

    g_object_bind_property(self, "volume",
                           priv->playbin, "volume",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
    g_object_bind_property(self, "uri",
                           priv->playbin, "uri",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
}

G_MODULE_EXPORT
void
peas_register_types(PeasObjectModule* module)
{
    gt_player_backend_gstreamer_vaapi_register_type(G_TYPE_MODULE(module));

    peas_object_module_register_extension_type(module,
                                               GT_TYPE_PLAYER_BACKEND,
                                               GT_TYPE_PLAYER_BACKEND_GSTREAMER_VAAPI);
}
