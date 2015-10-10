#include "gt-player-clutter.h"
#include "gt-player.h"
#include "gt-app.h"
#include "gt-player-header-bar.h"
#include "gt-win.h"

static const ClutterColor bg_colour = {0x00, 0x00, 0x00, 0x00};

typedef struct
{
    ClutterGstPlayback* player;
    ClutterActor* actor;
    ClutterActor* stage;
    ClutterContent* content;

    GtkWidget* fullscreen_bar;
    ClutterActor* fullscreen_bar_actor;

    GtkWidget* buffer_bar;
    ClutterActor* buffer_actor;

    gdouble volume;
    GtChannel* open_channel;
    gboolean playing;

    gchar* current_uri;

    GtWin* win; // Save a reference
} GtPlayerClutterPrivate;

static void gt_player_init(GtPlayerInterface* player);

G_DEFINE_TYPE_WITH_CODE(GtPlayerClutter, gt_player_clutter, GTK_CLUTTER_TYPE_EMBED,
                        G_ADD_PRIVATE(GtPlayerClutter)
                        G_IMPLEMENT_INTERFACE(GT_TYPE_PLAYER, gt_player_init));

enum 
{
    PROP_0,
    PROP_VOLUME,
    PROP_OPEN_CHANNEL,
    PROP_PLAYING,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtPlayerClutter*
gt_player_clutter_new(void)
{
    return g_object_new(GT_TYPE_PLAYER_CLUTTER, 
                        NULL);
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
        gchar* text = g_strdup_printf("Buffered %d%%\n", (gint) (percent * 100));
        
        clutter_actor_show(priv->buffer_actor);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(priv->buffer_bar), percent);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(priv->buffer_bar), text);

        g_free(text);
    }
    else
    {
        clutter_actor_hide(priv->buffer_actor);
    }

    g_print("Buffer %d%%\n", (int) (percent * 100));
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
    g_print("%f\n", w);

    g_object_set(priv->buffer_actor, "x", (gfloat) (w / 2 - 100), "y", (gfloat) (h / 2 - 25), NULL);
}

static void
realise_cb(GtkWidget* widget,
           gpointer udata)
{
    GtPlayerClutter* self = GT_PLAYER_CLUTTER(widget);
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    priv->win = GT_WIN_TOPLEVEL(self);

    g_signal_connect(priv->win, "notify::fullscreen", G_CALLBACK(fullscreen_cb), self);
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
              GParamSpec* pspec)
{
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
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
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

    clutter_gst_player_set_playing(CLUTTER_GST_PLAYER(priv->player), FALSE);
    clutter_gst_playback_set_uri(priv->player, NULL);

    priv->playing = FALSE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_PLAYING]);
}

static void
gt_player_init(GtPlayerInterface* iface)
{
    iface->set_uri = set_uri;
    iface->play = play;
    iface->stop = stop;
}

static void
gt_player_clutter_class_init(GtPlayerClutterClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

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

    g_object_class_override_property(object_class, PROP_VOLUME, "volume");
    g_object_class_override_property(object_class, PROP_OPEN_CHANNEL, "open-channel");
    g_object_class_override_property(object_class, PROP_PLAYING, "playing");
}

static void
gt_player_clutter_init(GtPlayerClutter* self)
{
    GtPlayerClutterPrivate* priv = gt_player_clutter_get_instance_private(self);

    priv->stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(self));
    priv->player = clutter_gst_playback_new();
    priv->actor = clutter_actor_new();
    priv->content = clutter_gst_aspectratio_new();
    priv->fullscreen_bar = GTK_WIDGET(gt_player_header_bar_new());
    priv->fullscreen_bar_actor = gtk_clutter_actor_new_with_contents(priv->fullscreen_bar);
    priv->buffer_bar = gtk_progress_bar_new();
    priv->buffer_actor = gtk_clutter_actor_new_with_contents(priv->buffer_bar);
    priv->playing = FALSE;

    g_object_set(priv->buffer_bar, "show-text", TRUE, NULL);

    gtk_clutter_embed_set_use_layout_size(GTK_CLUTTER_EMBED(self), TRUE);

    g_object_set(priv->buffer_actor,
                 "height", 30.0,
                 "width", 200.0,
                 NULL);
    clutter_actor_hide(priv->buffer_actor);

    g_object_set(priv->fullscreen_bar, "player", self, NULL);
    clutter_actor_hide(priv->fullscreen_bar_actor);

    g_object_set(priv->content, "player", priv->player, NULL);
    g_object_set(priv->actor, "content", priv->content, NULL);

    clutter_actor_add_child(priv->stage, priv->actor);
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

    g_object_bind_property(self, "volume",
                           priv->player, "audio-volume",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(priv->stage, "width",
                           priv->fullscreen_bar_actor, "width",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(priv->stage, "width",
                           priv->actor, "width",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(priv->stage, "height",
                           priv->actor, "height",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

    g_settings_bind(main_app->settings, "volume",
                    self, "volume",
                    G_SETTINGS_BIND_DEFAULT);
}
