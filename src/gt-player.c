#include "gt-player.h"
#include "gt-win.h"
#include <gdk/gdkx.h>
#include <locale.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>

typedef struct
{
    GtkWidget* fullscreen_bar_revealer;
    GtkWidget* fullscreen_bar;

    GtkWidget* player_box;

    GSettings* settings;

    GstElement* playbin;

    gboolean playing;
    gdouble volume;

    gulong motion_notify_hndl_id;

    GtTwitchStream* twitch_stream;
} GtPlayerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtPlayer, gt_player, GTK_TYPE_OVERLAY)

enum 
{
    PROP_0,
    PROP_PLAYING,
    PROP_VOLUME,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtPlayer*
gt_player_new(void)
{
    return g_object_new(GT_TYPE_PLAYER, 
                        NULL);
}

static void
unfullscreen_button_cb(GtkButton* button,
                       gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    gtk_window_unfullscreen(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))));
}

static void
fullscreen_cb(GtkWidget* widget,
              GParamSpec* evt,
              gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    gboolean fullscreen;

    g_object_get(widget, "fullscreen", &fullscreen, NULL);

    if (fullscreen)
    {
        gtk_widget_set_visible(priv->fullscreen_bar_revealer, TRUE);
        g_signal_handler_unblock(priv->player_box, priv->motion_notify_hndl_id);
    }
    else
    {
        gtk_widget_set_visible(priv->fullscreen_bar_revealer, FALSE);
        g_signal_handler_block(priv->player_box, priv->motion_notify_hndl_id);
    }
}

static gboolean
motion_notify_cb(GtkWidget* widget,
                 GdkEventMotion* evt,
                 gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->fullscreen_bar_revealer), evt->y <= 50);

    return FALSE;
}

static void
realize_cb(GtkWidget* widget,
           gpointer udata)
{
    GdkWindow* win = gtk_widget_get_window(widget);
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    gulong xid = gdk_x11_window_get_xid(win);

    gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(priv->playbin), xid);
    /* gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(priv->playsink), xid); */

    g_signal_connect(gtk_widget_get_toplevel(GTK_WIDGET(self)), "notify::fullscreen",
                     G_CALLBACK(fullscreen_cb), self);
}

static void
finalize(GObject* object)
{
    GtPlayer* self = (GtPlayer*) object;
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    G_OBJECT_CLASS(gt_player_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtPlayer* self = GT_PLAYER(obj);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    switch (prop)
    {
        case PROP_PLAYING:
            g_value_set_boolean(val, priv->playing);
            break;
        case PROP_VOLUME:
            g_value_set_double(val, priv->playing);
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
    GtPlayer* self = GT_PLAYER(obj);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    switch (prop)
    {
        case PROP_PLAYING:
            priv->playing = g_value_get_boolean(val); //TODO: Actually play/stop player
            if (!priv->playing)
                gt_player_play(self);
            else
                gt_player_stop(self);
            break;
        case PROP_VOLUME:
            priv->volume = g_value_get_double(val);
            g_object_set(priv->playbin, "volume", priv->volume, NULL);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_player_class_init(GtPlayerClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), 
                                                "/com/gnome-twitch/ui/gt-player.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, fullscreen_bar_revealer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, fullscreen_bar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, player_box);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), unfullscreen_button_cb);

    props[PROP_PLAYING] = g_param_spec_boolean("playing",
                                               "Playing",
                                               "True if playing, false if stopped",
                                               TRUE,
                                               G_PARAM_READWRITE);
    props[PROP_VOLUME] = g_param_spec_double("volume",
                                             "Volume",
                                             "Volume of player",
                                             0, 1, 0.3,
                                             G_PARAM_READWRITE);

    g_object_class_install_properties(object_class,
                                      NUM_PROPS,
                                      props);
}

/*
 * static void
 * pad_added_cb(GstElement* src,
 *              GstPad* pad,
 *              gpointer udata)
 * {
 *     GtPlayer* self = GT_PLAYER(udata);
 *     GtPlayerPrivate* priv = gt_player_get_instance_private(self);
 *     GstCaps* caps;
 *     GstStructure* strt;
 *     const gchar* type;
 *     GstPad* sink_pad = gst_element_get_static_pad(priv->atee, "sink");
 *     GstPad* sink_pad1 = gst_element_get_static_pad(priv->vtee, "sink");
 * 
 *     g_print("New pad %s from %s\n", GST_PAD_NAME(pad), GST_ELEMENT_NAME(src));
 * 
 *     caps = gst_pad_get_current_caps(pad);
 *     strt = gst_caps_get_structure(caps, 0);
 *     type = gst_structure_get_name(strt);
 * 
 *     g_print("Type %s\n", type);
 * 
 *     if (g_strcmp0(type, "video/x-raw") == 0)
 *     {
 *         gst_pad_link(pad, sink_pad1);
 *         [> gst_element_link_pads(priv->vtee, "src_0", priv->filebin, "videosink"); <]
 *     }
 *     if (g_strcmp0(type, "audio/x-raw") == 0)
 *     {
 *         gst_pad_link(pad, sink_pad);
 *         [> gst_element_link_pads(priv->atee, "src_0", priv->filebin, "audiosink"); <]
 *     }
 * 
 * }
 */

static void
gt_player_init(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    gtk_widget_init_template(GTK_WIDGET(self));

    GdkVisual* visual = gdk_screen_get_system_visual(gtk_widget_get_screen(GTK_WIDGET(self)));
    if (visual)
        gtk_widget_set_visual(GTK_WIDGET(self), visual);

    gtk_widget_add_events(GTK_WIDGET(priv->player_box), GDK_POINTER_MOTION_MASK);

    g_signal_connect(priv->player_box, "realize", G_CALLBACK(realize_cb), self);
    priv->motion_notify_hndl_id = g_signal_connect(priv->player_box,
                                                   "motion-notify-event",
                                                   G_CALLBACK(motion_notify_cb),
                                                   self);
    g_signal_handler_block(priv->player_box, priv->motion_notify_hndl_id);

    priv->playbin = gst_element_factory_make("playbin", NULL);
    priv->settings = g_settings_new("com.gnome-twitch.app");
}


void
gt_player_open_twitch_stream(GtPlayer* self, GtTwitchStream* stream)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    gchar* status;
    gchar* name;
    gchar* display_name;
    gchar* token;
    gchar* sig;
    GtTwitchStreamData* stream_data;
    GVariant* default_quality;
    GtTwitchStreamQuality _default_quality;
    GAction* quality_action;
    GtWin* win;

    priv->twitch_stream = stream;

    g_object_get(stream, 
                 "display-name", &display_name,
                 "name", &name,
                 "status", &status,
                 NULL);

    default_quality = g_settings_get_value(priv->settings, "default-quality");
    _default_quality = g_settings_get_enum(priv->settings, "default-quality");

    win = GT_WIN(gtk_widget_get_toplevel(GTK_WIDGET(self)));
    quality_action = g_action_map_lookup_action(G_ACTION_MAP(win), "player_set_quality");
    g_action_change_state(quality_action, default_quality);

    gtk_header_bar_set_title(GTK_HEADER_BAR(priv->fullscreen_bar), status);
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(priv->fullscreen_bar), display_name);

    gt_twitch_stream_access_token(main_app->twitch, name, &token, &sig);
    stream_data = gt_twitch_stream_by_quality(main_app->twitch,
                                              name,
                                              _default_quality,
                                              token, sig);

    g_object_set(priv->playbin, "uri", stream_data->url, NULL);

    gt_player_play(self);

    gt_twitch_stream_free(stream_data);
}

void
gt_player_play(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    gst_element_set_state(priv->playbin, GST_STATE_PLAYING);
    /* gst_element_set_state(priv->pipeline, GST_STATE_PLAYING); */
}

void
gt_player_pause(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    gst_element_set_state(priv->playbin, GST_STATE_PAUSED);
}

void
gt_player_stop(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    /* GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(priv->pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "gst-debug"); */

    gst_element_set_state(priv->playbin, GST_STATE_READY);
    /* gst_element_set_state(priv->pipeline, GST_STATE_READY); */
}

void
gt_player_set_quality(GtPlayer* self, GtTwitchStreamQuality qual)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    gchar* name;
    gchar* token;
    gchar* sig;
    GtTwitchStreamData* stream_data;

    g_object_get(priv->twitch_stream, 
                 "name", &name,
                 NULL);

    gt_twitch_stream_access_token(main_app->twitch, name, &token, &sig);
    stream_data = gt_twitch_stream_by_quality(main_app->twitch,
                                              name,
                                              qual,
                                              token, sig);

    gt_player_stop(self);
    g_object_set(priv->playbin, "uri", stream_data->url, NULL);
    gt_player_play(self);

    g_free(name);
    g_free(token);
    g_free(sig);
}
