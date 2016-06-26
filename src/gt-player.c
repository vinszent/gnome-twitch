#include "gt-player.h"
#include "gt-twitch.h"
#include "gt-win.h"
#include "gt-app.h"
#include "gt-enums.h"
#include "gt-chat.h"
#include "utils.h"
#include "gnome-twitch/gt-player-backend.h"
#include <libpeas-gtk/peas-gtk.h>
#include <glib/gi18n.h>

#define FULLSCREEN_BAR_REVEAL_HEIGHT 50

typedef struct
{
    GSimpleActionGroup* action_group;

    GtTwitchStreamQuality quality;

    GtChatViewSettings* chat_settings;

    GtkWidget* empty_box;
    GtkWidget* player_overlay;
    GtkWidget* docking_pane;
    GtkWidget* chat_view;
    GtkWidget* fullscreen_bar_revealer;
    GtkWidget* fullscreen_bar;
    GtkWidget* buffer_revealer;
    GtkWidget* buffer_label;

    GtPlayerBackend* backend;
    PeasPluginInfo* backend_info;

    GtChannel* channel;

    gdouble volume;
    gdouble muted;
    gboolean playing;
    gdouble chat_opacity;
    gdouble chat_width;
    gdouble chat_height;
    gdouble chat_x;
    gdouble chat_y;
    gboolean chat_docked;
    gboolean chat_visible;
    gboolean chat_dark_theme;
} GtPlayerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtPlayer, gt_player, GTK_TYPE_BIN)

enum
{
    PROP_0,
    PROP_VOLUME,
    PROP_MUTED,
    PROP_CHANNEL,
    PROP_PLAYING,
    PROP_CHAT_VISIBLE,
    PROP_CHAT_DOCKED,
    PROP_CHAT_WIDTH,
    PROP_CHAT_HEIGHT,
    PROP_CHAT_X,
    PROP_CHAT_Y,
    PROP_CHAT_DARK_THEME,
    PROP_CHAT_OPACITY,
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
update_chat_size(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    gboolean exists = priv->chat_view ? TRUE : FALSE;
    gboolean realised = exists ? gtk_widget_get_realized(priv->chat_view) : FALSE;

    if (!(exists && realised))
    {
        g_debug("{GtPlayer} Unable to set chat size, exists=%s, realised=%s",
                PRINT_BOOL(exists),
                PRINT_BOOL(realised));
        return;
    }

    if (priv->chat_docked)
    {
        gtk_widget_set_size_request(priv->chat_view,
                                    -1, -1);
    }
    else
    {
        GtkAllocation alloc;

        gtk_widget_get_allocation(GTK_WIDGET(self), &alloc);

        gtk_widget_set_size_request(priv->chat_view,
                                    priv->chat_width*alloc.width,
                                    priv->chat_height*alloc.height);
   }
}

static void
update_chat_pos(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    gboolean exists = priv->chat_view ? TRUE : FALSE;
    gboolean realised = exists ? gtk_widget_get_realized(priv->chat_view) : FALSE;

    if (!(exists && realised))
    {
        g_debug("{GtPlayer} Unable to set chat position, exists=%s, realised=%s",
                PRINT_BOOL(exists),
                PRINT_BOOL(realised));
        return;
    }

    if (priv->chat_docked)
    {
        gtk_widget_set_margin_start(priv->chat_view, 0.0);
        gtk_widget_set_margin_top(priv->chat_view, 0.0);
    }
    else
    {
        GtkAllocation alloc;

        gtk_widget_get_allocation(GTK_WIDGET(self), &alloc);
        gtk_widget_set_margin_start(priv->chat_view, priv->chat_x*alloc.width);
        gtk_widget_set_margin_top(priv->chat_view, priv->chat_y*alloc.height);
    }
}

static void
update_docked(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (priv->chat_docked)
    {
        gtk_widget_set_valign(priv->chat_view, GTK_ALIGN_FILL);
        gtk_widget_set_halign(priv->chat_view, GTK_ALIGN_FILL);

        update_chat_size(self);
        update_chat_pos(self);

        g_object_set(priv->chat_view, "opacity", 1.0, NULL);

        gtk_container_remove(GTK_CONTAINER(priv->player_overlay), priv->chat_view);
        gtk_paned_pack2(GTK_PANED(priv->docking_pane), priv->chat_view, FALSE, FALSE);
    }
    else
    {
        gtk_widget_set_valign(priv->chat_view, GTK_ALIGN_START);
        gtk_widget_set_halign(priv->chat_view, GTK_ALIGN_START);

        update_chat_size(self);
        update_chat_pos(self);

        g_object_set(priv->chat_view, "opacity", priv->chat_opacity, NULL);

        gtk_container_remove(GTK_CONTAINER(priv->docking_pane), priv->chat_view);
        gtk_overlay_add_overlay(GTK_OVERLAY(priv->player_overlay), priv->chat_view);
    }
}

static gboolean
motion_cb(GtkWidget* widget,
          GdkEventMotion* evt,
          gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (gt_win_is_fullscreen(GT_WIN_TOPLEVEL(widget)) && evt->y < FULLSCREEN_BAR_REVEAL_HEIGHT)
    {
        gtk_widget_set_visible(priv->fullscreen_bar_revealer, TRUE);
        gtk_revealer_set_reveal_child(GTK_REVEALER(priv->fullscreen_bar_revealer), TRUE);
    }
    else
    {
        gtk_revealer_set_reveal_child(GTK_REVEALER(priv->fullscreen_bar_revealer), FALSE);
    }

    return GDK_EVENT_STOP;
}

static void
buffer_fill_cb(GObject* source,
                  GParamSpec* pspec,
                  gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    gdouble perc;

    g_object_get(priv->backend, "buffer-fill", &perc, NULL);

    if (perc < 1.0)
    {
        gchar* text;

        text = g_strdup_printf(_("Buffered %d%%"), (gint) (perc*100.0));
        gtk_label_set_label(GTK_LABEL(priv->buffer_label), text);
        g_free(text);

        if (!gtk_revealer_get_child_revealed(GTK_REVEALER(priv->buffer_revealer)))
            gtk_revealer_set_reveal_child(GTK_REVEALER(priv->buffer_revealer), TRUE);
    }
    else
        gtk_revealer_set_reveal_child(GTK_REVEALER(priv->buffer_revealer), FALSE);

}

static void
revealer_revealed_cb(GObject* source,
                     GParamSpec* pspec,
                     gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (!gtk_revealer_get_child_revealed(GTK_REVEALER(source)))
        gtk_widget_set_visible(GTK_WIDGET(source), FALSE);
}

static void
fullscreen_cb(GObject* source,
              GParamSpec* pspec,
              gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (!gt_win_is_fullscreen(GT_WIN_TOPLEVEL(self)))
        gtk_revealer_set_reveal_child(GTK_REVEALER(priv->fullscreen_bar_revealer), FALSE);
}

static void
set_property(GObject* obj,
             guint prop,
             const GValue* val,
             GParamSpec* pspec)
{
    GtPlayer* self = GT_PLAYER(obj);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    switch (prop)
    {
        case PROP_VOLUME:
            priv->volume = g_value_get_double(val);
            break;
        case PROP_MUTED:
            priv->muted = g_value_get_boolean(val);
            break;
        case PROP_PLAYING:
            priv->playing = g_value_get_boolean(val);
            break;
        case PROP_CHANNEL:
            g_clear_object(&priv->channel);
            priv->channel = g_value_dup_object(val);
            break;
        case PROP_CHAT_WIDTH:
            priv->chat_width = g_value_get_double(val);
            update_chat_size(self);
            break;
        case PROP_CHAT_HEIGHT:
            priv->chat_height = g_value_get_double(val);
            update_chat_size(self);
            break;
        case PROP_CHAT_DOCKED:
            priv->chat_docked = g_value_get_boolean(val);
            update_docked(self);
            break;
        case PROP_CHAT_X:
            priv->chat_x = g_value_get_double(val);
            update_chat_pos(self);
            break;
        case PROP_CHAT_Y:
            priv->chat_y = g_value_get_double(val);
            update_chat_pos(self);
            break;
        case PROP_CHAT_OPACITY:
            priv->chat_opacity = g_value_get_double(val);
            break;
        case PROP_CHAT_VISIBLE:
            priv->chat_visible = g_value_get_boolean(val);
            break;
        case PROP_CHAT_DARK_THEME:
            priv->chat_dark_theme = g_value_get_boolean(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);

    }
}

static void
get_property(GObject* obj,
             guint prop,
             GValue* val,
             GParamSpec* pspec)
{
    GtPlayer* self = GT_PLAYER(obj);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    switch (prop)
    {
        case PROP_VOLUME:
            g_value_set_double(val, priv->volume);
            break;
        case PROP_MUTED:
            g_value_set_boolean(val, priv->muted);
            break;
        case PROP_CHANNEL:
            g_value_set_object(val, priv->channel);
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
        case PROP_CHAT_OPACITY:
            g_value_set_double(val, priv->chat_opacity);
            break;
        case PROP_CHAT_VISIBLE:
            g_value_set_boolean(val, priv->chat_visible);
            break;
        case PROP_CHAT_DARK_THEME:
            g_value_set_boolean(val, priv->chat_dark_theme);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
streams_list_cb(GObject* source,
                GAsyncResult* res,
                gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    GList* streams;
    GtTwitchStreamData* stream;
    GtTwitchStreamQuality default_quality;

    streams = g_task_propagate_pointer(G_TASK(res), NULL); //TODO: Error handling

    if (!streams)
    {
        g_warning("{GtPlayer} Error opening stream");
        return;
    }

    stream = gt_twitch_stream_list_filter_quality(streams, priv->quality);

    g_object_set(self, "playing", FALSE, NULL);
    g_object_set(priv->backend, "uri", stream->url, NULL);
    g_object_set(self, "playing", TRUE, NULL);
}

static void
realise_cb(GtkWidget* widget,
            gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    GtWin* win = GT_WIN_TOPLEVEL(self);

    gtk_widget_insert_action_group(GTK_WIDGET(win), "player",
                                   G_ACTION_GROUP(priv->action_group));

    gtk_widget_realize(priv->fullscreen_bar);

    g_signal_connect(win, "notify::fullscreen", G_CALLBACK(fullscreen_cb), self);
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

    if (eval->value != priv->quality)
        gt_player_set_quality(self, eval->value);

    g_simple_action_set_state(action, arg);

    g_type_class_unref(eclass);
}

static void
plugin_loaded_cb(PeasEngine* engine,
                 PeasPluginInfo* info,
                 gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (peas_engine_provides_extension(engine, info, GT_TYPE_PLAYER_BACKEND))
    {
        PeasExtension* ext;

        g_message("{GtPlayer} Loaded player backend '%s'", peas_plugin_info_get_name(info));

        if (priv->backend_info)
        {
            peas_engine_unload_plugin(main_app->plugins_engine,
                                      priv->backend_info);
        }

        ext = peas_engine_create_extension(engine, info,
                                           GT_TYPE_PLAYER_BACKEND,
                                           NULL);

        priv->backend = GT_PLAYER_BACKEND(ext);
        priv->backend_info = g_boxed_copy(PEAS_TYPE_PLUGIN_INFO,
                                          info);

        g_signal_connect(priv->backend, "notify::buffer-fill", G_CALLBACK(buffer_fill_cb), self);

        g_object_bind_property(self, "volume",
                               priv->backend, "volume",
                               G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
        g_object_bind_property(self, "muted",
                               priv->backend, "muted",
                               G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
        g_object_bind_property(self, "playing",
                               priv->backend, "playing",
                               G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
        g_object_bind_property(self, "chat-opacity",
                               priv->chat_view, "opacity",
                               G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
        g_object_bind_property(self, "chat-dark-theme",
                               priv->chat_view, "dark-theme",
                               G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
        g_object_bind_property(self, "chat-visible",
                               priv->chat_view, "visible",
                               G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

        gtk_container_remove(GTK_CONTAINER(priv->player_overlay), priv->empty_box);

        GtkWidget* widget = gt_player_backend_get_widget(priv->backend);
        gtk_widget_add_events(widget, GDK_POINTER_MOTION_MASK);
        gtk_container_add(GTK_CONTAINER(priv->player_overlay), widget);
        gtk_widget_show_all(priv->player_overlay);
    }
}

static void
plugin_unloaded_cb(PeasEngine* engine,
                   PeasPluginInfo* info,
                   gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (peas_engine_provides_extension(engine, info, GT_TYPE_PLAYER_BACKEND))
    {
        g_message("{GtPlayer} Unloaded player backend '%s'", peas_plugin_info_get_name(info));

        gtk_container_remove(GTK_CONTAINER(priv->player_overlay),
                             gt_player_backend_get_widget(priv->backend));
        gtk_container_add(GTK_CONTAINER(priv->player_overlay),
                          priv->empty_box);

        g_clear_object(&priv->backend);

        g_boxed_free(PEAS_TYPE_PLUGIN_INFO, priv->backend_info);
        priv->backend_info = NULL;
    }
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
    props[PROP_MUTED] = g_param_spec_boolean("muted",
                                             "Muted",
                                             "Whether muted",
                                             FALSE,
                                             G_PARAM_READWRITE);
    props[PROP_CHANNEL] = g_param_spec_object("channel",
                                              "Channel",
                                              "Current channel",
                                              GT_TYPE_CHANNEL,
                                              G_PARAM_READWRITE);
    props[PROP_PLAYING] = g_param_spec_boolean("playing",
                                               "Playing",
                                               "Whether playing",
                                               FALSE,
                                               G_PARAM_READWRITE);
    props[PROP_CHAT_VISIBLE] = g_param_spec_boolean("chat-visible",
                                                    "Chat Visible",
                                                    "Whether chat visible",
                                                    TRUE,
                                                    G_PARAM_READWRITE);
    props[PROP_CHAT_DOCKED] = g_param_spec_boolean("chat-docked",
                                                   "Chat Docked",
                                                   "Whether chat docked",
                                                   TRUE,
                                                   G_PARAM_READWRITE);
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
    props[PROP_CHAT_X] = g_param_spec_double("chat-x",
                                             "Chat X",
                                             "Current chat x",
                                             0, 1.0, 0.2,
                                             G_PARAM_READWRITE);
    props[PROP_CHAT_Y] = g_param_spec_double("chat-y",
                                             "Chat Y",
                                             "Current chat y",
                                             0, 1.0, 0.2,
                                             G_PARAM_READWRITE);
    props[PROP_CHAT_DARK_THEME] = g_param_spec_boolean("chat-dark-theme",
                                                       "Chat Dark Theme",
                                                       "Whether chat dark theme",
                                                       TRUE,
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_CHAT_OPACITY] = g_param_spec_double("chat-opacity",
                                                   "Chat Opacity",
                                                   "Current chat opacity",
                                                   0, 1.0, 1.0,
                                                   G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, NUM_PROPS, props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), "/com/gnome-twitch/ui/gt-player.ui");
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, empty_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, docking_pane);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, player_overlay);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, fullscreen_bar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, fullscreen_bar_revealer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, buffer_revealer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayer, buffer_label);
}

static GActionEntry actions[] =
{
    //TODO: Make these GPropertyAction?
    {"set_quality", NULL, "s", "'source'", set_quality_action_cb},
};

static void
chat_settings_changed_cb(GObject* source,
                         GParamSpec* pspec,
                         gpointer udata)
{
    GtPlayer* self = GT_PLAYER(udata);
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (!priv->chat_settings)
        return;

    g_object_get(G_OBJECT(self),
                 "chat-docked", &priv->chat_settings->docked,
                 "chat-visible", &priv->chat_settings->visible,
                 "chat-width", &priv->chat_settings->width,
                 "chat-height", &priv->chat_settings->height,
                 "chat-x", &priv->chat_settings->x_pos,
                 "chat-y", &priv->chat_settings->y_pos,
                 "chat-dark-theme", &priv->chat_settings->dark_theme,
                 "chat-opacity", &priv->chat_settings->opacity,
                 NULL);
}

static void
gt_player_init(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    gtk_widget_init_template(GTK_WIDGET(self));

    g_object_ref(priv->empty_box);

    priv->chat_view = GTK_WIDGET(gt_chat_new());
    g_object_ref(priv->chat_view); //TODO: Unref in finalise
    gtk_widget_show_all(priv->chat_view);

    priv->action_group = g_simple_action_group_new();
    g_action_map_add_action_entries(G_ACTION_MAP(priv->action_group), actions,
                                    G_N_ELEMENTS(actions), self);
    g_action_map_add_action(G_ACTION_MAP(priv->action_group),
                            G_ACTION(g_property_action_new("show_chat", G_OBJECT(self), "chat-visible")));
    g_action_map_add_action(G_ACTION_MAP(priv->action_group),
                            G_ACTION(g_property_action_new("dock_chat", G_OBJECT(self), "chat-docked")));
    g_action_map_add_action(G_ACTION_MAP(priv->action_group),
                            G_ACTION(g_property_action_new("dark_theme_chat", G_OBJECT(self), "chat-dark-theme")));

    utils_signal_connect_oneshot(self, "realize", G_CALLBACK(realise_cb), self);
    g_signal_connect(priv->fullscreen_bar_revealer, "notify::child-revealed", G_CALLBACK(revealer_revealed_cb), self);
    g_signal_connect(priv->buffer_revealer, "notify::child-revealed", G_CALLBACK(revealer_revealed_cb), self);
    g_signal_connect(self, "motion-notify-event", G_CALLBACK(motion_cb), self);
    g_signal_connect(self, "notify::chat-docked", G_CALLBACK(chat_settings_changed_cb), self);
    g_signal_connect(self, "notify::chat-visible", G_CALLBACK(chat_settings_changed_cb), self);
    g_signal_connect(self, "notify::chat-width", G_CALLBACK(chat_settings_changed_cb), self);
    g_signal_connect(self, "notify::chat-height", G_CALLBACK(chat_settings_changed_cb), self);
    g_signal_connect(self, "notify::chat-x", G_CALLBACK(chat_settings_changed_cb), self);
    g_signal_connect(self, "notify::chat-y", G_CALLBACK(chat_settings_changed_cb), self);
    g_signal_connect(self, "notify::chat-dark-theme", G_CALLBACK(chat_settings_changed_cb), self);
    g_signal_connect(self, "notify::chat-opacity", G_CALLBACK(chat_settings_changed_cb), self);
    g_signal_connect_after(main_app->plugins_engine, "load-plugin", G_CALLBACK(plugin_loaded_cb), self);
    g_signal_connect(main_app->plugins_engine, "unload-plugin", G_CALLBACK(plugin_unloaded_cb), self);

    gchar** c;
    gchar** _c;
    for (_c = c = peas_engine_get_loaded_plugins(main_app->plugins_engine); *c != NULL; c++)
    {
        PeasPluginInfo* info = peas_engine_get_plugin_info(main_app->plugins_engine, *c);

        if (peas_engine_provides_extension(main_app->plugins_engine, info, GT_TYPE_PLAYER_BACKEND))
            plugin_loaded_cb(main_app->plugins_engine, info, self);
    }

    g_strfreev(_c);
}

void
gt_player_play(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (!priv->backend)
        g_message("{GtPlayer} Can't play, no backend loaded");
    else
        g_object_set(priv->backend, "playing", TRUE, NULL);
}

void
gt_player_stop(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    if (!priv->backend)
        g_message("{GtPlayer} Can't stop, no backend loaded");
    else
        g_object_set(priv->backend, "playing", FALSE, NULL);
}

void
gt_player_open_channel(GtPlayer* self, GtChannel* chan)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    gchar* name;
    gchar* token;
    gchar* sig;
    GVariant* default_quality;
    GAction* quality_action;

    if (!priv->backend)
    {
        g_message("{GtPlayer} Can't open channel, no backend loaded");
        return;
    }

    gtk_label_set_label(GTK_LABEL(priv->buffer_label), _("Loading stream"));

    gtk_widget_set_visible(priv->buffer_revealer, TRUE);

    if (!gtk_revealer_get_child_revealed(GTK_REVEALER(priv->buffer_revealer)))
        gtk_revealer_set_reveal_child(GTK_REVEALER(priv->buffer_revealer), TRUE);

    g_object_set(self, "channel", chan, NULL);

    g_object_get(chan,
                 "name", &name,
                 NULL);

    gt_chat_connect(GT_CHAT(priv->chat_view), name);

    default_quality = g_settings_get_value(main_app->settings, "default-quality");
    priv->quality = g_settings_get_enum(main_app->settings, "default-quality");

    g_message("{GtPlayer} Opening stream '%s' with quality '%s'", name, g_variant_get_string(default_quality, NULL));

    quality_action = g_action_map_lookup_action(G_ACTION_MAP(priv->action_group), "set_quality");
    g_action_change_state(quality_action, default_quality);

    priv->chat_settings = g_hash_table_lookup(main_app->chat_settings_table, name);
    if (!priv->chat_settings)
    {
        priv->chat_settings = gt_chat_view_settings_new();
        g_hash_table_insert(main_app->chat_settings_table, g_strdup(name), priv->chat_settings);
    }

    g_signal_handlers_block_by_func(self, chat_settings_changed_cb, self);
    g_object_set(G_OBJECT(self),
                 "chat-docked", priv->chat_settings->docked,
                 "chat-width", priv->chat_settings->width,
                 "chat-height", priv->chat_settings->height,
                 "chat-dark-theme", priv->chat_settings->dark_theme,
                 NULL);
    g_object_set(G_OBJECT(self), // These props need to be set after
                 "chat-x", priv->chat_settings->x_pos,
                 "chat-y", priv->chat_settings->y_pos,
                 "chat-visible", priv->chat_settings->visible,
                 "chat-opacity", priv->chat_settings->opacity,
                 NULL);
    g_signal_handlers_unblock_by_func(self, chat_settings_changed_cb, self);

    gt_twitch_all_streams_async(main_app->twitch, name, NULL, (GAsyncReadyCallback) streams_list_cb, self);

    g_free(name);
}

void
gt_player_close_channel(GtPlayer* self)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);

    g_object_set(self, "channel", NULL, NULL);

    gt_chat_disconnect(GT_CHAT(priv->chat_view));

    gt_player_stop(self);
}

void
gt_player_set_quality(GtPlayer* self, GtTwitchStreamQuality qual)
{
    GtPlayerPrivate* priv = gt_player_get_instance_private(self);
    gchar* name;
    GtTwitchStreamData* stream_data;
    GtChannel* chan;

    g_object_get(self, "channel", &chan, NULL);
    g_object_get(chan, "name", &name, NULL);

    priv->quality = qual;

    gt_twitch_all_streams_async(main_app->twitch, name, NULL, (GAsyncReadyCallback) streams_list_cb, self);

    g_free(name);
    g_object_unref(chan);
}

void
gt_player_toggle_muted(GtPlayer* self)
{
    gboolean muted;

    g_object_get(self, "muted", &muted, NULL);
    g_object_set(self, "muted", !muted, NULL);
}
