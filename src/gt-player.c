#include "gt-player.h"
#include "gt-twitch.h"
#include "gt-win.h"
#include "gt-app.h"

G_DEFINE_INTERFACE(GtPlayer, gt_player, G_TYPE_OBJECT)

enum 
{
    PROP_0,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];
static GtPlayerInterface* gt_player_parent_interface = NULL;

GtPlayer*
gt_player_new(void)
{
    return g_object_new(GT_TYPE_PLAYER, 
                        NULL);
}

static void
gt_player_default_init(GtPlayerInterface* iface)
{
    g_object_interface_install_property(iface,
                                        g_param_spec_double("volume",
                                                            "Volume",
                                                            "Volume of player",
                                                            0, 1.0, 0.3,
                                                            G_PARAM_READWRITE));
    g_object_interface_install_property(iface,
                                        g_param_spec_object("open-channel",
                                                            "Open channel",
                                                            "Currently open channel",
                                                            GT_TYPE_CHANNEL,
                                                            G_PARAM_READWRITE));
    g_object_interface_install_property(iface,
                                        g_param_spec_boolean("playing",
                                                             "Playing",
                                                             "Whether playing",
                                                             FALSE,
                                                             G_PARAM_READABLE));
}

void
gt_player_play(GtPlayer* self)
{
    GT_PLAYER_GET_IFACE(self)->play(self);
}

void
gt_player_stop(GtPlayer* self)
{
    GT_PLAYER_GET_IFACE(self)->stop(self);
}

void
gt_player_open_channel(GtPlayer* self, GtChannel* chan)
{
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

    g_object_set(self, "open-channel", chan, NULL);

    g_object_get(chan, 
                 "display-name", &display_name,
                 "name", &name,
                 "status", &status,
                 NULL);

    default_quality = g_settings_get_value(main_app->settings, "default-quality");
    _default_quality = g_settings_get_enum(main_app->settings, "default-quality");

    win = GT_WIN(gtk_widget_get_toplevel(GTK_WIDGET(self)));
    quality_action = g_action_map_lookup_action(G_ACTION_MAP(win), "player_set_quality");
    g_action_change_state(quality_action, default_quality);

    gt_twitch_stream_access_token(main_app->twitch, name, &token, &sig);
    stream_data = gt_twitch_stream_by_quality(main_app->twitch,
                                              name,
                                              _default_quality,
                                              token, sig);

    GT_PLAYER_GET_IFACE(self)->set_uri(self, stream_data->url);
    GT_PLAYER_GET_IFACE(self)->play(self);

    gt_twitch_stream_free(stream_data);
    g_free(name);
    g_free(status);
    g_free(token);
    g_free(sig);
}

void
gt_player_set_quality(GtPlayer* self, GtTwitchStreamQuality qual)
{
    gchar* name;
    gchar* token;
    gchar* sig;
    GtTwitchStreamData* stream_data;
    GtChannel* chan;

    g_object_get(self, "open-channel", &chan, NULL);
    g_object_get(chan, "name", &name, NULL);

    gt_twitch_stream_access_token(main_app->twitch, name, &token, &sig);
    stream_data = gt_twitch_stream_by_quality(main_app->twitch,
                                              name, qual,
                                              token, sig);

    GT_PLAYER_GET_IFACE(self)->stop(self);
    GT_PLAYER_GET_IFACE(self)->set_uri(self, stream_data->url);
    GT_PLAYER_GET_IFACE(self)->play(self);

    g_free(name);
    g_free(token);
    g_free(sig);
    g_object_unref(chan);
    gt_twitch_stream_free(stream_data);
}
