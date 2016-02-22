#include "gt-player-clutter.h"
#include "gt-player.h"
#include "gt-app.h"
#include "gt-player-header-bar.h"
#include "gt-win.h"
#include "gt-twitch-chat-view.h"
#include "gt-enums.h"
#include "utils.h"
#include <glib/gprintf.h>

#define CURSOR_HIDING_TIMEOUT 2

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

    GtkWidget* buffer_box;
    GtkWidget* buffer_label;
    GtkWidget* buffer_spinner;
    ClutterActor* buffer_actor;

    GtkWidget* chat_view;
    ClutterActor* chat_actor;

    gdouble volume;
    GtChannel* open_channel;
    gboolean playing;

    gchar* current_uri;

    gdouble chat_width;
    gdouble chat_height;
    gdouble chat_x;
    gdouble chat_y;
    gboolean chat_docked;
    gboolean chat_visible;

    GtWin* win; // Save a reference

    guint inhibitor_cookie;
    guint cursor_hiding_timeout_id;
} GtPlayerClutterPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtPlayerClutter, gt_player_clutter, GT_TYPE_PLAYER)

enum
{
    PROP_0,
    PROP_VOLUME,
    PROP_OPEN_CHANNEL,
    PROP_PLAYING,
    PROP_CHAT_OPACITY,
    PROP_CHAT_DARK_THEME,
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
scale_chat_view_cb(GObject* source,
                   GParamSpec* pspec,
                   gpointer udata)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(udata);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);
    gfloat stage_width, stage_height, new_height, new_width, chat_height, chat_width, chat_x, chat_y, new_x, new_y;

    g_object_get(priv->stage,
                 "width", &stage_width,
                 "height", &stage_height,
                 NULL);
    g_object_get(priv->chat_actor,
                 "width", &chat_width,
                 "height", &chat_height,
                 "x", &chat_x,
                 "y", &chat_y,
                 NULL);
    new_width = (gfloat) stage_width * priv->chat_width;
    new_height = (gfloat) stage_height * priv->chat_height;

    new_x = (gfloat) priv->chat_x*stage_width;
    new_y = (gfloat) priv->chat_y*stage_height;

    if (new_x < 0)
        new_x = 0;
    else if (new_x + chat_width > stage_width)
        new_x = stage_width - chat_width;

    if (new_y < 0)
        new_y = 0;
    else if (new_y + chat_height > stage_height)
        new_y = stage_height - chat_height;

    g_object_set(priv->chat_actor,
                 "width", new_width,
                 "height", new_height,
                 "x", new_x,
                 "y", new_y,
                 NULL);
}

static void
update_chat_view_size_cb(GObject* source,
                         GParamSpec* pspec,
                         gpointer udata)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(udata);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);
    gfloat stage_width, stage_height, new_height, new_width, chat_height, chat_width, chat_x, chat_y, new_x, new_y;
    //TODO: Cleanup & simplify?

    g_object_get(priv->stage,
                 "width", &stage_width,
                 "height", &stage_height,
                 NULL);
    g_object_get(priv->chat_actor,
                 "width", &chat_width,
                 "height", &chat_height,
                 "x", &chat_x,
                 "y", &chat_y,
                 NULL);

    new_width = (gfloat) stage_width * priv->chat_width;
    new_height = (gfloat) stage_height * priv->chat_height;

    new_x = (gfloat) chat_x - (new_width - chat_width) / 2.0f;
    new_y = (gfloat) chat_y - (new_height - chat_height) / 2.0f;

    if (new_x < 0)
        new_x = 0;
    else if (new_x + chat_width > stage_width)
        new_x = stage_width - chat_width;

    if (new_y < 0)
        new_y = 0;
    else if (new_y + chat_height > stage_height)
        new_y = stage_height - chat_height;

    priv->chat_x = (gdouble) new_x / stage_width;
    priv->chat_y = (gdouble) new_y / stage_height;

    g_object_set(priv->chat_actor,
                 "width", new_width,
                 "height", new_height,
                 "x", new_x,
                 "y", new_y,
                 NULL);

    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_CHAT_X]);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_CHAT_Y]);
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

    g_object_set(priv->chat_actor,
                 "x", (gfloat) priv->chat_x*stage_width,
                 "y", (gfloat) priv->chat_y*stage_height,
                 NULL);
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
        clutter_actor_insert_child_below(priv->stage, priv->chat_actor, priv->fullscreen_bar_actor);
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

    priv->inhibitor_cookie = gtk_application_inhibit(GTK_APPLICATION(main_app),
                                                     GTK_WINDOW(priv->win),
                                                     GTK_APPLICATION_INHIBIT_IDLE,
                                                     "Displaying a stream");

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

    if (priv->inhibitor_cookie != 0)
    {
        gtk_application_uninhibit(GTK_APPLICATION(main_app), priv->inhibitor_cookie);
        priv->inhibitor_cookie = 0;
    }
}

static void
channel_set_cb(GObject* source,
               GParamSpec* pspec,
               gpointer udata)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(udata);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    if (priv->open_channel)
    {
        gtk_label_set_text(GTK_LABEL(priv->buffer_label), "Loading stream");
        clutter_actor_show(priv->buffer_actor);

        gt_twitch_chat_view_connect(GT_TWITCH_CHAT_VIEW(priv->chat_view),
                                    gt_channel_get_name(priv->open_channel));
    }
    else
    {
        gt_twitch_chat_view_disconnect(GT_TWITCH_CHAT_VIEW(priv->chat_view));
    }

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

static void
unschedule_cursor_hiding(GtPlayerClutter* self)
{
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    if (priv->cursor_hiding_timeout_id != 0)
    {
        g_source_remove(priv->cursor_hiding_timeout_id);
        priv->cursor_hiding_timeout_id = 0;
    }
}

static gboolean
hide_cursor_timeout_cb(GtPlayerClutter* self)
{
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    clutter_stage_hide_cursor(CLUTTER_STAGE(priv->stage));
    unschedule_cursor_hiding(self);
    return G_SOURCE_REMOVE;
}

static void
schedule_cursor_hiding(GtPlayerClutter* self)
{
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    unschedule_cursor_hiding(self);
    priv->cursor_hiding_timeout_id = g_timeout_add_seconds(CURSOR_HIDING_TIMEOUT,
                                                           (GSourceFunc) hide_cursor_timeout_cb,
                                                           self);
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
            clutter_stage_show_cursor(stage);
            schedule_cursor_hiding(self);

            if (gt_win_get_fullscreen(priv->win) && ((ClutterMotionEvent*) event)->y < 50)
                show_fullscreen_bar(self);
            else
                hide_fullscreen_bar(self);

            handled = TRUE;
            break;
        }
        case CLUTTER_ENTER:
        {
            schedule_cursor_hiding(self);
            handled = TRUE;
            break;
        }
        case CLUTTER_LEAVE:
        {
            unschedule_cursor_hiding(self);
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

    percent = clutter_gst_playback_get_buffer_fill(priv->player);

    if (percent < 1.0)
    {
        gchar text[20];
        g_sprintf(text, "Buffered %d%%", (gint) (percent * 100));

        gtk_label_set_text(GTK_LABEL(priv->buffer_label), text);
        clutter_actor_show(priv->buffer_actor);
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
anchored_cb(GtkWidget* widget,
            GtkWidget* prev_toplevel,
            gpointer udata)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(widget);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    priv->win = GT_WIN_TOPLEVEL(self);

    g_signal_connect(priv->win, "notify::fullscreen", G_CALLBACK(fullscreen_cb), self);
    g_signal_connect(self, "notify::chat-docked", G_CALLBACK(chat_docked_cb), self);

    g_signal_handlers_disconnect_by_func(self, anchored_cb, udata); //One-shot
}

static void
finalize(GObject* object)
{
    GtPlayerClutter* self = (GtPlayerClutter*) object;
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    unschedule_cursor_hiding(self);

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
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_CHAT_Y] = g_param_spec_double("chat-y",
                                             "Chat Y",
                                             "Current chat y",
                                             0, G_MAXDOUBLE, 0.0,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
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
    priv->buffer_spinner = gtk_spinner_new();
    priv->buffer_label = gtk_label_new(NULL);
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

    clutter_actor_hide(priv->fullscreen_bar_actor);

    g_object_set(priv->buffer_spinner,
                 "width-request", 32,
                 "height-request", 32,
                 "margin-top", 7,
                 "margin-left", 7,
                 "margin-right", 7,
                 "active", TRUE,
                 NULL);
    g_object_set(priv->buffer_label,
                 "margin-bottom", 7,
                 "margin-left", 7,
                 "margin-right", 7,
                 NULL);

    gtk_container_add(GTK_CONTAINER(priv->buffer_box), priv->buffer_spinner);
    gtk_container_add(GTK_CONTAINER(priv->buffer_box), priv->buffer_label);
    gtk_widget_show_all(priv->buffer_box);

//    gtk_clutter_embed_set_use_layout_size(GTK_CLUTTER_EMBED(priv->root_widget), TRUE);

    /* g_object_set(priv->buffer_actor, */
    /*              "height", 100.0, */
    /*              "width", 150.0, */
    /*              NULL); */
    g_object_set(priv->buffer_actor,
                 "margin-top", 10.0,
                 "margin-bottom", 10.0,
                 "margin-left", 10.0,
                 "margin-right", 10.0,
                 NULL);

    g_object_set(priv->video_actor,
                 "x-expand", TRUE,
                 "y-expand", TRUE,
                 NULL);

    clutter_actor_hide(priv->buffer_actor);

    g_object_set(priv->fullscreen_bar, "player", self, NULL);

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

    g_signal_connect(self, "hierarchy-changed", G_CALLBACK(anchored_cb), self);
    g_signal_connect(priv->player, "notify::buffer-fill", G_CALLBACK(buffer_fill_cb), self);
    g_signal_connect(priv->stage, "notify::size", G_CALLBACK(size_changed_cb), self);
    g_signal_connect(priv->stage, "event", G_CALLBACK(clutter_stage_event_cb), self);
    g_signal_connect(priv->root_widget, "button-press-event", G_CALLBACK(button_press_cb), self);
    g_signal_connect(priv->stage, "notify::size", G_CALLBACK(scale_chat_view_cb), self);
    g_signal_connect(self, "notify::chat-width", G_CALLBACK(update_chat_view_size_cb), self);
    g_signal_connect(self, "notify::chat-height", G_CALLBACK(update_chat_view_size_cb), self);
    g_signal_connect(self, "notify::chat-x", G_CALLBACK(update_chat_view_pos_cb), self);
    g_signal_connect(self, "notify::chat-y", G_CALLBACK(update_chat_view_pos_cb), self);
    g_signal_connect(self, "notify::open-channel", G_CALLBACK(channel_set_cb), self);

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
