#include "gt-player-header-bar.h"
#include "gt-player.h"
#include "gt-win.h"

typedef struct
{
    GtPlayer* player;

    gchar* channel_name;
    gchar* channel_status;

    GtkWidget* status_label;
    GtkWidget* name_label;

    GtkWidget* fullscreen_button;

    GtkWidget* play_image;
    GtkWidget* stop_image;
    GtkWidget* fullscreen_image;
    GtkWidget* unfullscreen_image;

    GtkWidget* play_stop_button;

    GtkWidget* volume_button;

    GMenu* hamburger_menu;

    GtkAdjustment* chat_view_width_adjustment;
    GtkAdjustment* chat_view_height_adjustment;
    GtkAdjustment* chat_view_opacity_adjustment;

    gboolean fullscreen;
} GtPlayerHeaderBarPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtPlayerHeaderBar, gt_player_header_bar, GTK_TYPE_HEADER_BAR)

enum
{
    PROP_0,
    PROP_PLAYER,
    PROP_CHANNEL_NAME,
    PROP_CHANNEL_STATUS,
    PROP_FULLSCREEN,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtPlayerHeaderBar*
gt_player_header_bar_new(void)
{
    return g_object_new(GT_TYPE_PLAYER_HEADER_BAR,
                        NULL);
}

static void
fullscreen_cb(GtkWidget* widget,
              GParamSpec* pspec,
              gpointer udata)
{
    GtPlayerHeaderBar* self = GT_PLAYER_HEADER_BAR(udata);
    GtPlayerHeaderBarPrivate* priv = gt_player_header_bar_get_instance_private(self);

    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(self), !priv->fullscreen);

    if (priv->fullscreen)
        gtk_button_set_image(GTK_BUTTON(priv->fullscreen_button), priv->unfullscreen_image);
    else
        gtk_button_set_image(GTK_BUTTON(priv->fullscreen_button), priv->fullscreen_image);
}

static void
player_back_button_cb(GtPlayerHeaderBar* self,
                      GtkButton* button)
{
    GtPlayerHeaderBarPrivate* priv = gt_player_header_bar_get_instance_private(self);
    GtWin* win = GT_WIN(gtk_widget_get_toplevel(GTK_WIDGET(self)));

    gt_win_browse_view(win);
    gt_player_stop(GT_PLAYER(priv->player));
}

static void
player_play_stop_button_cb(GtPlayerHeaderBar* self,
                           GtkButton* button)
{
    GtPlayerHeaderBarPrivate* priv = gt_player_header_bar_get_instance_private(self);

    gboolean playing;

    g_object_get(priv->player, "playing", &playing, NULL);

    if (playing)
        gt_player_stop(GT_PLAYER(priv->player));
    else
        gt_player_play(GT_PLAYER(priv->player));
}

static void
playing_cb(GObject* source,
           GParamSpec* pspec,
           gpointer udata)
{
    GtPlayerHeaderBar* self = GT_PLAYER_HEADER_BAR(udata);
    GtPlayerHeaderBarPrivate* priv = gt_player_header_bar_get_instance_private(self);

    gboolean playing;

    g_object_get(priv->player, "playing", &playing, NULL);

    if (playing)
        gtk_button_set_image(GTK_BUTTON(priv->play_stop_button), priv->stop_image);
    else
        gtk_button_set_image(GTK_BUTTON(priv->play_stop_button), priv->play_image);
}

static void
player_fullscreen_button_cb(GtPlayerHeaderBar* self,
                            GtkButton* button)
{
    GtPlayerHeaderBarPrivate* priv = gt_player_header_bar_get_instance_private(self);

    if (priv->fullscreen)
        gtk_window_unfullscreen(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))));
    else
        gtk_window_fullscreen(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))));
}

static void
opened_channel_cb(GObject* source,
                  GParamSpec* pspec,
                  gpointer udata)
{
    GtPlayerHeaderBar* self = GT_PLAYER_HEADER_BAR(udata);
    GtChannel* chan = NULL;
    gchar* name;
    gchar* status;

    g_object_get(source, "open-channel", &chan, NULL);
    g_object_get(chan,
                 "display-name", &name,
                 "status", &status,
                 NULL);

    g_object_set(self,
                 "name", name,
                 "status", status,
                 NULL);

    g_free(name);
    g_free(status);
    g_object_unref(chan);
}

static void
player_set_cb(GObject* source,
              GParamSpec* pspec,
              gpointer udata)
{
    GtPlayerHeaderBar* self = GT_PLAYER_HEADER_BAR(udata);
    GtPlayerHeaderBarPrivate* priv = gt_player_header_bar_get_instance_private(self);


    if (priv->player)
    {
        g_object_bind_property(priv->player, "volume",
                               priv->volume_button, "value",
                               G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
        g_object_bind_property(priv->chat_view_opacity_adjustment, "value",
                               priv->player, "chat-opacity",
                               G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
        g_object_bind_property(priv->chat_view_width_adjustment, "value",
                               priv->player, "chat-width",
                               G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
        g_object_bind_property(priv->chat_view_height_adjustment, "value",
                               priv->player, "chat-height",
                               G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

        g_signal_connect(priv->player, "notify::open-channel", G_CALLBACK(opened_channel_cb), self);
        g_signal_connect(priv->player, "notify::playing", G_CALLBACK(playing_cb), self);
    }
}

static void
finalize(GObject* object)
{
    GtPlayerHeaderBar* self = (GtPlayerHeaderBar*) object;
    GtPlayerHeaderBarPrivate* priv = gt_player_header_bar_get_instance_private(self);

    G_OBJECT_CLASS(gt_player_header_bar_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtPlayerHeaderBar* self = GT_PLAYER_HEADER_BAR(obj);
    GtPlayerHeaderBarPrivate* priv = gt_player_header_bar_get_instance_private(self);

    switch (prop)
    {
        case PROP_CHANNEL_NAME:
            g_value_set_string(val, priv->channel_name);
            break;
        case PROP_CHANNEL_STATUS:
            g_value_set_string(val, priv->channel_status);
            break;
        case PROP_PLAYER:
            g_value_set_object(val, priv->player);
            break;
        case PROP_FULLSCREEN:
            g_value_set_boolean(val, priv->fullscreen);
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
    GtPlayerHeaderBar* self = GT_PLAYER_HEADER_BAR(obj);
    GtPlayerHeaderBarPrivate* priv = gt_player_header_bar_get_instance_private(self);

    switch (prop)
    {
        case PROP_CHANNEL_NAME:
            if (priv->channel_name)
                g_free(priv->channel_name);
            priv->channel_name = g_value_dup_string(val);
            gtk_header_bar_set_subtitle(GTK_HEADER_BAR(self), priv->channel_name);
            break;
        case PROP_CHANNEL_STATUS:
            if (priv->channel_status)
                g_free(priv->channel_status);
            priv->channel_status = g_value_dup_string(val);
            gtk_header_bar_set_title(GTK_HEADER_BAR(self), priv->channel_status);
            break;
        case PROP_PLAYER:
            if (priv->player)
                g_object_unref(priv->player);
            priv->player = g_value_dup_object(val);
            break;
        case PROP_FULLSCREEN:
            priv->fullscreen = g_value_get_boolean(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
realize(GtkWidget* widget,
         gpointer udata)
{
    GtPlayerHeaderBar* self = GT_PLAYER_HEADER_BAR(widget);
    GtPlayerHeaderBarPrivate* priv = gt_player_header_bar_get_instance_private(self);

    g_object_bind_property(GT_WIN_TOPLEVEL(self), "fullscreen",
                           self, "fullscreen",
                           G_BINDING_SYNC_CREATE);

}

static void
gt_player_header_bar_class_init(GtPlayerHeaderBarClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_PLAYER] = g_param_spec_object("player",
                                             "Player",
                                             "Associated player",
                                             GT_TYPE_PLAYER,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_CHANNEL_NAME] = g_param_spec_string("name",
                                           "Name",
                                           "Name of channel",
                                           NULL,
                                           G_PARAM_WRITABLE);
    props[PROP_CHANNEL_STATUS] = g_param_spec_string("status",
                                             "Status",
                                             "Staus of channel",
                                             NULL,
                                             G_PARAM_WRITABLE);
    props[PROP_FULLSCREEN] = g_param_spec_boolean("fullscreen",
                                                  "Fullscreen",
                                                  "Whether in fullscreen",
                                                  FALSE,
                                                  G_PARAM_READWRITE);

    g_object_class_install_properties(object_class,
                                      NUM_PROPS,
                                      props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
                                                "/com/gnome-twitch/ui/gt-player-header-bar.ui");
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, fullscreen_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, play_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, stop_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, fullscreen_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, unfullscreen_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, volume_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, play_stop_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, chat_view_width_adjustment);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, chat_view_height_adjustment);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, chat_view_opacity_adjustment);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), player_fullscreen_button_cb);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), player_back_button_cb);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), player_play_stop_button_cb);
}

static void
gt_player_header_bar_init(GtPlayerHeaderBar* self)
{
    GtPlayerHeaderBarPrivate* priv = gt_player_header_bar_get_instance_private(self);

    gtk_widget_init_template(GTK_WIDGET(self));

    gtk_style_context_remove_class(gtk_widget_get_style_context(priv->volume_button), GTK_STYLE_CLASS_FLAT);

    g_signal_connect(self, "realize", G_CALLBACK(realize), NULL);
    g_signal_connect(self, "notify::fullscreen", G_CALLBACK(fullscreen_cb), self);
    g_signal_connect(self, "notify::player", G_CALLBACK(player_set_cb), self);

}
