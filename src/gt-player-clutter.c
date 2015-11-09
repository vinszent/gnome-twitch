#include "gt-player-clutter.h"
#include "gt-player.h"
#include "gt-app.h"
#include "gt-player-header-bar.h"
#include "gt-win.h"
#include "gt-twitch-chat-view.h"
#include "gt-enums.h"
#include "utils.h"
#include <glib/gprintf.h>

static const ClutterColor bg_colour = {0x00, 0x00, 0x00, 0x00};

typedef struct
{
    GtkWidget* root_widget;

    ClutterActor* docked_layour_actor;

    ClutterGstPlayback* player;
    ClutterActor* video_actor;
    ClutterActor* stage;
    ClutterContent* content;

    GtkWidget* fullscreen_bar;
    ClutterActor* fullscreen_bar_actor;

    GtkWidget* buffer_bar;
    GtkWidget* buffer_box;
    ClutterActor* buffer_actor;

    GtkWidget* chat_view;
    ClutterActor* chat_actor;

    gdouble volume;
    GtChannel* open_channel;
    gboolean playing;

    gchar* current_uri;

    gdouble chat_opacity;
    gdouble chat_width;
    gdouble chat_height;
    gdouble chat_x;
    gdouble chat_y;
    gboolean chat_docked;
    gboolean chat_visible;

    GtWin* win; // Save a reference
} GtPlayerClutterPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtPlayerClutter, gt_player_clutter, GT_TYPE_PLAYER)

enum
{
    PROP_0,
    PROP_VOLUME,
    PROP_OPEN_CHANNEL,
    PROP_PLAYING,
    PROP_CHAT_OPACITY,
    PROP_CHAT_WIDTH,
    PROP_CHAT_HEIGHT,
    PROP_CHAT_DOCKED,
    PROP_CHAT_X,
    PROP_CHAT_Y,
    PROP_CHAT_VISIBLE,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtPlayerClutter*
gt_player_clutter_new(void)
{
    return g_object_new(GT_TYPE_PLAYER_CLUTTER,
                        NULL);
}

static GtkWidget*
get_chat_view(GtPlayer* player)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(player);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    return priv->chat_view;
}

static void
update_chat_view_size_cb(GObject* source,
                         GParamSpec* pspec,
                         gpointer udata)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(udata);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);
    gfloat stage_width, stage_height;

    g_object_get(priv->stage,
                 "width", &stage_width,
                 "height", &stage_height,
                 NULL);

    g_object_set(priv->chat_actor,
                 "width", (gfloat) stage_width * priv->chat_width,
                 "height", (gfloat) stage_height * priv->chat_height,
                 NULL);
}

static void
update_chat_view_pos_cb(GObject* source,
                        GParamSpec* pspec,
                        gpointer udata)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(udata);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);
    gfloat stage_width, stage_height;

    g_object_get(priv->stage,
                 "width", &stage_width,
                 "height", &stage_height,
                 NULL);

    clutter_actor_set_x(priv->chat_actor, (gfloat) priv->chat_x*stage_width);
    clutter_actor_set_y(priv->chat_actor, (gfloat) priv->chat_y*stage_height);
}

static void
chat_docked_cb(GObject* source,
               GParamSpec* pspec,
               gpointer udata)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(udata);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    if (priv->chat_docked)
    {
        clutter_actor_remove_child(priv->stage, priv->chat_actor);
        clutter_actor_add_child(priv->docked_layour_actor, priv->chat_actor);
    }
    else
    {
        clutter_actor_remove_child(priv->docked_layour_actor, priv->chat_actor);
        clutter_actor_add_child(priv->stage, priv->chat_actor);
    }
}

static void
hide_fullscreen_bar(GtPlayerClutter* self)
{
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    clutter_actor_hide(priv->fullscreen_bar_actor);
}

static void
show_fullscreen_bar(GtPlayerClutter* self)
{
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    clutter_actor_show(priv->fullscreen_bar_actor);
}

static void
set_uri(GtPlayer* player, const gchar* uri)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(player);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    g_free(priv->current_uri);
    priv->current_uri = g_strdup(uri);
}

static void
play(GtPlayer* player)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(player);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    clutter_gst_playback_set_uri(priv->player, priv->current_uri);
    clutter_gst_player_set_playing(CLUTTER_GST_PLAYER(priv->player), TRUE);

    priv->playing = TRUE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_PLAYING]);
}

static void
stop(GtPlayer* player)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(player);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    clutter_gst_playback_set_uri(priv->player, NULL);
    clutter_gst_player_set_playing(CLUTTER_GST_PLAYER(priv->player), FALSE);

    priv->playing = FALSE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_PLAYING]);
}

static void
fullscreen_cb(GObject* source,
              GParamSpec* pspec,
              gpointer udata)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(udata);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    if (!gt_win_get_fullscreen(priv->win))
        hide_fullscreen_bar(self);
}

static gboolean
clutter_stage_event_cb(ClutterStage* stage,
                       ClutterEvent* event,
                       gpointer udata)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(udata);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);
    gboolean handled = FALSE;

    switch (event->type)
    {
        case CLUTTER_MOTION:
        {
            if (gt_win_get_fullscreen(priv->win) && ((ClutterMotionEvent*) event)->y < 50)
                show_fullscreen_bar(self);
            else
                hide_fullscreen_bar(self);

            handled = TRUE;
            break;
        }
        default:
        break;
    }

    return handled;
}

static void
buffer_fill_cb(GObject* source,
               GParamSpec* pspec,
               gpointer udata)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(udata);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);
    gdouble percent;

    g_object_get(priv->player, "buffer-fill", &percent, NULL);

    if (percent < 1.0)
    {
        gchar text[20];
        g_sprintf(text, "Buffered %d%%\n", (gint) (percent * 100));

        clutter_actor_show(priv->buffer_actor);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(priv->buffer_bar), percent);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(priv->buffer_bar), text);
    }
    else
        clutter_actor_hide(priv->buffer_actor);
}

static void
size_changed_cb(GObject* source,
                GParamSpec* pspec,
                gpointer udata)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(udata);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);
    gfloat w, h;

    clutter_actor_get_size(priv->stage, &w, &h);

    g_object_set(priv->buffer_actor, "x", (gfloat) (w / 2 - 100), "y", (gfloat) (h / 2 - 25), NULL);
}

static gboolean
button_press_cb(GtkWidget* widget,
                GdkEventButton* evt,
                gpointer udata)
{
    GtWin* win = GT_WIN_TOPLEVEL(widget);

    if (evt->type == GDK_2BUTTON_PRESS)
    {
        if (gt_win_get_fullscreen(win))
            gtk_window_unfullscreen(GTK_WINDOW(win));
        else
            gtk_window_fullscreen(GTK_WINDOW(win));
    }

    return TRUE;
}

static void
realise_cb(GtkWidget* widget,
           gpointer udata)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(widget);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    priv->win = GT_WIN_TOPLEVEL(self);

    g_signal_connect(priv->win, "notify::fullscreen", G_CALLBACK(fullscreen_cb), self);
    g_signal_connect(self, "notify::chat-docked", G_CALLBACK(chat_docked_cb), self);
}

static void
finalize(GObject* object)
{
    GtPlayerClutter* self = (GtPlayerClutter*) object;
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    G_OBJECT_CLASS(gt_player_clutter_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec){

    GtPlayerClutter* self = GT_PLAYER_CLUTTER(obj);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    switch (prop)
    {
        case PROP_VOLUME:
            g_value_set_double(val, priv->volume);
            break;
        case PROP_OPEN_CHANNEL:
            g_value_set_object(val, priv->open_channel);
            break;
        case PROP_PLAYING:
            g_value_set_boolean(val, priv->playing);
            break;
        case PROP_CHAT_WIDTH:
            g_value_set_double(val, priv->chat_width);
            break;
        case PROP_CHAT_HEIGHT:
            g_value_set_double(val, priv->chat_height);
            break;
        case PROP_CHAT_DOCKED:
            g_value_set_boolean(val, priv->chat_docked);
            break;
        case PROP_CHAT_X:
            g_value_set_double(val, priv->chat_x);
            break;
        case PROP_CHAT_Y:
            g_value_set_double(val, priv->chat_y);
            break;
        case PROP_CHAT_VISIBLE:
            g_value_set_boolean(val, priv->chat_visible);
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
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(obj);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    switch (prop)
    {
        case PROP_VOLUME:
            priv->volume = g_value_get_double(val);
            break;
        case PROP_OPEN_CHANNEL:
            g_clear_object(&priv->open_channel);
            priv->open_channel = g_value_dup_object(val);
            break;
        case PROP_CHAT_WIDTH:
            priv->chat_width = g_value_get_double(val);
            break;
        case PROP_CHAT_HEIGHT:
            priv->chat_height = g_value_get_double(val);
            break;
        case PROP_CHAT_DOCKED:
            priv->chat_docked = g_value_get_boolean(val);
            break;
        case PROP_CHAT_X:
            priv->chat_x = g_value_get_double(val);
            break;
        case PROP_CHAT_Y:
            priv->chat_y = g_value_get_double(val);
            break;
        case PROP_CHAT_VISIBLE:
            priv->chat_visible = g_value_get_boolean(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_player_clutter_class_init(GtPlayerClutterClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    GtPlayerClass* player_class = GT_PLAYER_CLASS(klass);

    clutter_gst_init(NULL, NULL);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    player_class->set_uri = set_uri;
    player_class->play = play;
    player_class->stop = stop;
    player_class->get_chat_view = get_chat_view;

    props[PROP_VOLUME] = g_param_spec_double("volume",
                                             "Volume",
                                             "Volume of player",
                                             0.0, 1.0, 0.3,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_OPEN_CHANNEL] = g_param_spec_object("open-channel",
                                                   "Open Channel",
                                                   "Currently open channel",
                                                   GT_TYPE_CHANNEL,
                                                   G_PARAM_READWRITE);
    props[PROP_PLAYING] = g_param_spec_boolean("playing",
                                               "Playing",
                                               "Whether playing",
                                               FALSE,
                                               G_PARAM_READABLE);
    props[PROP_CHAT_WIDTH] = g_param_spec_double("chat-width",
                                                 "Chat Width",
                                                 "Current chat width",
                                                 0, 1.0, 0.2,
                                                 G_PARAM_READWRITE);
    props[PROP_CHAT_HEIGHT] = g_param_spec_double("chat-height",
                                                  "Chat Height",
                                                  "Current chat height",
                                                  0, 1.0, 1.0,
                                                  G_PARAM_READWRITE);
    props[PROP_CHAT_DOCKED] = g_param_spec_boolean("chat-docked",
                                                   "Chat Docked",
                                                   "Whether chat docked",
                                                   TRUE,
                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_CHAT_X] = g_param_spec_double("chat-x",
                                             "Chat X",
                                             "Current chat x",
                                             0, G_MAXDOUBLE, 0,
                                             G_PARAM_READWRITE);
    props[PROP_CHAT_Y] = g_param_spec_double("chat-y",
                                             "Chat Y",
                                             "Current chat y",
                                             0, G_MAXDOUBLE, 0,
                                             G_PARAM_READWRITE);
    props[PROP_CHAT_VISIBLE] = g_param_spec_boolean("chat-visible",
                                                    "Chat Visible",
                                                    "Whether chat visible",
                                                    TRUE,
                                                    G_PARAM_READWRITE);

    g_object_class_override_property(object_class, PROP_VOLUME, "volume");
    g_object_class_override_property(object_class, PROP_OPEN_CHANNEL, "open-channel");
    g_object_class_override_property(object_class, PROP_PLAYING, "playing");
    g_object_class_override_property(object_class, PROP_CHAT_DOCKED, "chat-docked");
    g_object_class_override_property(object_class, PROP_CHAT_VISIBLE, "chat-visible");
    g_object_class_override_property(object_class, PROP_CHAT_X, "chat-x");
    g_object_class_override_property(object_class, PROP_CHAT_Y, "chat-y");
    //TODO Move these into GtPlayer
    g_object_class_install_property(object_class, PROP_CHAT_WIDTH, props[PROP_CHAT_WIDTH]);
    g_object_class_install_property(object_class, PROP_CHAT_HEIGHT, props[PROP_CHAT_HEIGHT]);
}

static void
gt_player_clutter_init(GtPlayerClutter* self)
{
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    priv->root_widget = gtk_clutter_embed_new();
    gtk_container_add(GTK_CONTAINER(self), priv->root_widget);
    priv->stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(priv->root_widget));
    priv->player = clutter_gst_playback_new();
    priv->video_actor = clutter_actor_new();
    priv->content = clutter_gst_aspectratio_new();
    priv->fullscreen_bar = GTK_WIDGET(gt_player_header_bar_new());
    priv->fullscreen_bar_actor = gtk_clutter_actor_new_with_contents(priv->fullscreen_bar);
    priv->buffer_bar = gtk_progress_bar_new();
    priv->buffer_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    priv->buffer_actor = gtk_clutter_actor_new_with_contents(priv->buffer_box);
    priv->chat_view = GTK_WIDGET(gt_twitch_chat_view_new());
    priv->chat_actor = gtk_clutter_actor_new_with_contents(priv->chat_view);
    priv->docked_layour_actor = clutter_actor_new();
    priv->playing = FALSE;

    g_object_ref_sink(G_OBJECT(priv->chat_actor));

    ClutterLayoutManager* layout = clutter_box_layout_new();
//    g_object_set(layout, "homogeneous", TRUE, NULL);

    clutter_actor_set_layout_manager(priv->docked_layour_actor, layout);

    gtk_container_add(GTK_CONTAINER(priv->buffer_box), priv->buffer_bar);
    gtk_widget_show_all(priv->buffer_box);
    g_object_set(priv->buffer_bar, "show-text", TRUE, "margin", 7, NULL);

//    gtk_clutter_embed_set_use_layout_size(GTK_CLUTTER_EMBED(priv->root_widget), TRUE);

    g_object_set(priv->buffer_actor,
                 "height", 50.0,
                 "width", 150.0,
                 NULL);

    g_object_set(priv->video_actor,
                 "x-expand", TRUE,
                 "y-expand", TRUE,
                 NULL);

    clutter_actor_hide(priv->buffer_actor);

    g_object_set(priv->fullscreen_bar, "player", self, NULL);
    clutter_actor_hide(priv->fullscreen_bar_actor);

    g_object_set(priv->content, "player", priv->player, NULL);
    g_object_set(priv->video_actor, "content", priv->content, NULL);

    clutter_actor_add_child(priv->docked_layour_actor, priv->video_actor);
    clutter_actor_add_child(priv->docked_layour_actor, priv->chat_actor);
    clutter_actor_add_child(priv->stage, priv->docked_layour_actor);
    clutter_actor_add_child(priv->stage, priv->fullscreen_bar_actor);
    clutter_actor_add_child(priv->stage, priv->buffer_actor);

    clutter_actor_set_background_color(priv->stage, &bg_colour);
    clutter_actor_set_opacity(priv->fullscreen_bar_actor, 200);

    /* clutter_gst_playback_set_buffering_mode(priv->player, CLUTTER_GST_BUFFERING_MODE_STREAM); // In-memory buffering (let user choose?) */
    clutter_gst_playback_set_buffering_mode(priv->player, CLUTTER_GST_BUFFERING_MODE_DOWNLOAD); // On-disk buffering

    g_signal_connect(priv->stage, "event", G_CALLBACK(clutter_stage_event_cb), self);
    g_signal_connect(self, "realize", G_CALLBACK(realise_cb), self);
    g_signal_connect(priv->player, "notify::buffer-fill", G_CALLBACK(buffer_fill_cb), self);
    g_signal_connect(priv->stage, "notify::size", G_CALLBACK(size_changed_cb), self);
    g_signal_connect(priv->root_widget, "button-press-event", G_CALLBACK(button_press_cb), self);
    g_signal_connect(priv->stage, "notify::size", G_CALLBACK(update_chat_view_size_cb), self);
    g_signal_connect(self, "notify::chat-width", G_CALLBACK(update_chat_view_size_cb), self);
    g_signal_connect(self, "notify::chat-height", G_CALLBACK(update_chat_view_size_cb), self);
    g_signal_connect(self, "notify::chat-x", G_CALLBACK(update_chat_view_pos_cb), self);
    g_signal_connect(self, "notify::chat-y", G_CALLBACK(update_chat_view_pos_cb), self);

    g_object_bind_property(self, "volume",
                           priv->player, "audio-volume",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(priv->stage, "width",
                           priv->fullscreen_bar_actor, "width",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(priv->stage, "width",
                           priv->docked_layour_actor, "width",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(priv->stage, "height",
                           priv->docked_layour_actor, "height",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(self, "chat-visible",
                           priv->chat_actor, "visible",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

    g_settings_bind(main_app->settings, "volume",
                    self, "volume",
                    G_SETTINGS_BIND_DEFAULT);

    ADD_STYLE_CLASS(self, "player-clutter");
    ADD_STYLE_CLASS(priv->buffer_box, "buffer-box");

    gtk_widget_show_all(GTK_WIDGET(self));
}
