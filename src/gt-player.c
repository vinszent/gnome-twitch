#include "gt-player.h"
#include "gt-twitch.h"
#include "gt-win.h"
#include "gt-app.h"
#include "gt-enums.h"

typedef struct
{
    GSimpleActionGroup* action_group;
} GtPlayerPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(GtPlayer, gt_player, GTK_TYPE_BIN)

enum
{
    PROP_0,
    PROP_VOLUME,
    PROP_OPEN_CHANNEL,
    PROP_PLAYING,
    PROP_CHAT_VISIBLE,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];


static void
finalise(GObject* obj)
{
    GtPlayer* self = GT_PLAYER(obj);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    G_OBJECT_CLASS(gt_player_parent_class)->finalize(obj);
}

static void
set_property(GObject* obj,
             guint prop,
             const GValue* val,
             GParamSpec* pspec)
{
    GtPlayer* self = GT_PLAYER(obj);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (prop <= PROP_CHAT_VISIBLE)
        g_warning("{GtPlayer} Property '%s' not overriden", pspec->name);
    else
        G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
}

static void
get_property(GObject* obj,
             guint prop,
             GValue* val,
             GParamSpec* pspec)
{
    GtPlayer* self = GT_PLAYER(obj);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (prop <= PROP_CHAT_VISIBLE)
        g_warning("{GtPlayer} Property '%s' not overriden", pspec->name);
    else
        G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
}

static void
realise_cb(GtkWidget* widget,
           gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    gtk_widget_insert_action_group(GTK_WIDGET(GT_WIN_TOPLEVEL(self)),
                                   "player", G_ACTION_GROUP(priv->action_group));

}

static void
set_quality_action_cb(GSimpleAction* action,
                      GVariant* arg,
                      gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    GEnumClass* eclass;
    GEnumValue* eval;

    eclass = g_type_class_ref(GT_TYPE_TWITCH_STREAM_QUALITY);
    eval = g_enum_get_value_by_nick(eclass, g_variant_get_string(arg, NULL));

    gt_player_set_quality(self, eval->value);

    g_simple_action_set_state(action, arg);

    g_type_class_unref(eclass);
}

static void
show_chat_action_cb(GSimpleAction* action,
                    GVariant* arg,
                    gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    g_object_set(self, "chat-visible", g_variant_get_boolean(arg), NULL);

    g_simple_action_set_state(action, arg);
}

static void
gt_player_class_init(GtPlayerClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalise;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_VOLUME] = g_param_spec_double("volume",
                                             "Volume",
                                             "Current volume",
                                             0, 1.0, 0.3,
                                             G_PARAM_READWRITE);
    props[PROP_OPEN_CHANNEL] = g_param_spec_object("open-channel",
                                                   "Open Channel",
                                                   "Current open channel",
                                                   GT_TYPE_CHANNEL,
                                                   G_PARAM_READWRITE);
    props[PROP_PLAYING] = g_param_spec_boolean("playing",
                                               "Playing",
                                               "Whether playing",
                                               FALSE,
                                               G_PARAM_READWRITE);
    //TODO Disconnect from chat when not visible?
    props[PROP_CHAT_VISIBLE] = g_param_spec_boolean("chat-visible",
                                                    "Chat Visible",
                                                    "Whether chat visible",
                                                    TRUE,
                                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    g_object_class_install_properties(object_class, NUM_PROPS, props);
}

static GActionEntry actions[] =
{
    {"set_quality", NULL, "s", "'source'", set_quality_action_cb},
    {"show_chat", NULL, NULL, "true", show_chat_action_cb},
};

static void
gt_player_init(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    priv->action_group = g_simple_action_group_new();
    g_action_map_add_action_entries(G_ACTION_MAP(priv->action_group), actions,
                                    G_N_ELEMENTS(actions), self);
    g_signal_connect(self, "realize", G_CALLBACK(realise_cb), self);
}

void
gt_player_play(GtPlayer* self)
{
    GT_PLAYER_GET_CLASS(self)->play(self);
}

void
gt_player_stop(GtPlayer* self)
{
    GT_PLAYER_GET_CLASS(self)->stop(self);
}

void
gt_player_open_channel(GtPlayer* self, GtChannel* chan)
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

    g_object_set(self, "open-channel", chan, NULL);

    g_object_get(chan,
                 "display-name", &display_name,
                 "name", &name,
                 "status", &status,
                 NULL);

    default_quality = g_settings_get_value(main_app->settings, "default-quality");
    _default_quality = g_settings_get_enum(main_app->settings, "default-quality");

    g_message("{GtPlayer} Opening channel '%s' with quality '%d'", name, _default_quality);

    quality_action = g_action_map_lookup_action(G_ACTION_MAP(priv->action_group), "set_quality");
    g_action_change_state(quality_action, default_quality);

    gt_twitch_stream_access_token(main_app->twitch, name, &token, &sig);
    stream_data = gt_twitch_stream_by_quality(main_app->twitch,
                                              name,
                                              _default_quality,
                                              token, sig);

    GT_PLAYER_GET_CLASS(self)->set_uri(self, stream_data->url);
    GT_PLAYER_GET_CLASS(self)->play(self);

    gt_twitch_stream_data_free(stream_data);
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

    GT_PLAYER_GET_CLASS(self)->stop(self);
    GT_PLAYER_GET_CLASS(self)->set_uri(self, stream_data->url);
    GT_PLAYER_GET_CLASS(self)->play(self);

    g_free(name);
    g_free(token);
    g_free(sig);
    g_object_unref(chan);
    gt_twitch_stream_data_free(stream_data);
}
