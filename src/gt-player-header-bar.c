#include "gt-player-header-bar.h"
#include "gt-player.h"
#include "gt-win.h"

typedef struct
{
    GtPlayer* player;

    gchar* stream_name;
    gchar* stream_status;

    GtkWidget* status_label;
    GtkWidget* name_label;

    GtkWidget* fullscreen_button;

    GtkWidget* play_image;
    GtkWidget* stop_image;
    GtkWidget* fullscreen_image;
    GtkWidget* unfullscreen_image;

    GtkWidget* volume_button;

    gboolean fullscreen;
} GtPlayerHeaderBarPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtPlayerHeaderBar, gt_player_header_bar, GTK_TYPE_HEADER_BAR)

enum 
{
    PROP_0,
    PROP_PLAYER,
    PROP_STREAM_NAME,
    PROP_STREAM_STATUS,
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

    if (!playing)
        gtk_button_set_image(button, priv->play_image);
    else
        gtk_button_set_image(button, priv->stop_image);

    g_object_set(priv->player, "playing", !playing, NULL);
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
        case PROP_STREAM_NAME:
            g_value_set_string(val, priv->stream_name);
            break;
        case PROP_STREAM_STATUS:
            g_value_set_string(val, priv->stream_status);
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
        case PROP_STREAM_NAME:
            if (priv->stream_name)
                g_free(priv->stream_name);
            priv->stream_name = g_value_dup_string(val);
            gtk_header_bar_set_subtitle(GTK_HEADER_BAR(self), priv->stream_name);
            break;
        case PROP_STREAM_STATUS:
            if (priv->stream_status)
                g_free(priv->stream_status);
            priv->stream_status = g_value_dup_string(val);
            gtk_header_bar_set_title(GTK_HEADER_BAR(self), priv->stream_status);
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
    GtkWidget* toplevel;

    g_object_bind_property(priv->volume_button, "value",
                           priv->player, "volume",
                           G_BINDING_BIDIRECTIONAL);

    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(self));

    g_object_bind_property(toplevel, "fullscreen",
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
                                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT);
    props[PROP_STREAM_NAME] = g_param_spec_string("name",
                                           "Name",
                                           "Name of stream",
                                           NULL,
                                           G_PARAM_WRITABLE);
    props[PROP_STREAM_STATUS] = g_param_spec_string("status",
                                             "Status",
                                             "Staus of stream",
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
                                                "/org/gnome/gnome-twitch/ui/gt-player-header-bar.ui");
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, fullscreen_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, play_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, stop_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, fullscreen_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, unfullscreen_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, volume_button);
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
}
