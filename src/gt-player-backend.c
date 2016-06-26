
#include "gnome-twitch/gt-player-backend.h"

G_DEFINE_INTERFACE(GtPlayerBackend, gt_player_backend, G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_VOLUME,
    PROP_MUTED,
    PROP_PLAYING,
    PROP_BUFFER_PERCENT,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

static void
gt_player_backend_default_init(GtPlayerBackendInterface* iface)
{
    g_object_interface_install_property(iface,
                                        g_param_spec_double("volume",
                                                            "Volume",
                                                            "Current volume",
                                                            0, 1.0, 0.3,
                                                            G_PARAM_READWRITE));
    g_object_interface_install_property(iface,
                                        g_param_spec_boolean("muted",
                                                             "Muted",
                                                             "Whether muted",
                                                             FALSE,
                                                             G_PARAM_READWRITE));
    g_object_interface_install_property(iface,
                                        g_param_spec_boolean("playing",
                                                             "Playing",
                                                             "Whether playing",
                                                             FALSE,
                                                             G_PARAM_READWRITE));
    g_object_interface_install_property(iface,
                                        g_param_spec_string("uri",
                                                            "Uri",
                                                            "Current uri",
                                                            NULL,
                                                            G_PARAM_READWRITE));
    g_object_interface_install_property(iface,
                                        g_param_spec_double("buffer-percent",
                                                            "buffer-percent",
                                                            "Current buffer percent",
                                                            0, 1.0, 0,
                                                            G_PARAM_READWRITE));
}

GtkWidget*
gt_player_backend_get_widget(GtPlayerBackend* backend)
{
    g_assert(GT_IS_PLAYER_BACKEND(backend));
    g_assert_nonnull(GT_PLAYER_BACKEND_GET_IFACE(backend)->get_widget);

    return GT_PLAYER_BACKEND_GET_IFACE(backend)->get_widget(backend);
}
