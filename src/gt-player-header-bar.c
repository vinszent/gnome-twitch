#include "gt-player-header-bar.h"
#include "gt-player.h"
#include "gt-win.h"

#define TAG "GtPlayerHeaderBar"
#include "utils.h"

typedef struct
{
    GtkWidget* status_label;
    GtkWidget* name_label;
    GtkWidget* title_button;
    GtkWidget* fullscreen_button;
    GtkWidget* show_chat_image;
    GtkWidget* hide_chat_image;
    GtkWidget* fullscreen_image;
    GtkWidget* unfullscreen_image;
    GtkWidget* back_button;
    GtkWidget* back_separator;
    GtkWidget* volume_button;

    GMenu* hamburger_menu;

    GtkWidget* edit_chat_button;
    GtkWidget* dock_chat_button;
    GtkWidget* show_chat_button;

    GtkAdjustment* chat_view_opacity_adjustment;
    GtkAdjustment* chat_view_width_adjustment;
    GtkAdjustment* chat_view_height_adjustment;
    GtkAdjustment* chat_view_x_adjustment;
    GtkAdjustment* chat_view_y_adjustment;
} GtPlayerHeaderBarPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtPlayerHeaderBar, gt_player_header_bar, GTK_TYPE_HEADER_BAR)

enum
{
    PROP_0,
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
    GtWin* win = GT_WIN_TOPLEVEL(self);

    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(self), !gt_win_is_fullscreen(win));

    if (gt_win_is_fullscreen(win))
        gtk_button_set_image(GTK_BUTTON(priv->fullscreen_button), priv->unfullscreen_image);
    else
        gtk_button_set_image(GTK_BUTTON(priv->fullscreen_button), priv->fullscreen_image);
}

static void
chat_visible_cb(GObject* source,
                GParamSpec* pspec,
                gpointer udata)
{
    GtPlayerHeaderBar* self = GT_PLAYER_HEADER_BAR(udata);
    GtPlayerHeaderBarPrivate* priv = gt_player_header_bar_get_instance_private(self);
    GtWin* win = GT_WIN_TOPLEVEL(self);
    gboolean visible;

    g_object_get(win->player, "chat-visible", &visible, NULL);

    if (visible)
        gtk_button_set_image(GTK_BUTTON(priv->show_chat_button), priv->hide_chat_image);
    else
        gtk_button_set_image(GTK_BUTTON(priv->show_chat_button), priv->show_chat_image);
}

static gboolean
mute_volume_cb(GtkWidget* button,
               GdkEventButton* evt,
               gpointer udata)
{
    GtPlayerHeaderBar* self = GT_PLAYER_HEADER_BAR(udata);
    GtPlayerHeaderBarPrivate* priv = gt_player_header_bar_get_instance_private(self);
    GtWin* win = GT_WIN_TOPLEVEL(self);

    if (evt->button == 3)
    {
        gt_player_toggle_muted(win->player);
        return TRUE;
    }

    return FALSE;
}

static void
player_channel_set_cb(GObject* source,
                      GParamSpec* spec,
                      gpointer udata)
{
    GtPlayerHeaderBar* self = GT_PLAYER_HEADER_BAR(udata);
    GtPlayerHeaderBarPrivate* priv = gt_player_header_bar_get_instance_private(self);
    GtWin* win = GT_WIN_TOPLEVEL(self);
    gchar* name;
    gchar* status;
    GtChannel* chan;

    g_object_get(win->player, "channel", &chan, NULL);

    if (chan)
    {
        g_object_get(chan,
                     "display-name", &name,
                     "status", &status,
                     NULL);

        gtk_label_set_label(GTK_LABEL(priv->status_label), status);
        gtk_label_set_label(GTK_LABEL(priv->name_label), name);

        g_object_unref(chan);
        g_free(name);
        g_free(status);
    }
}

static gboolean
chat_pos_upper_transformer(GBinding* binding,
                           const GValue* from,
                           GValue* to,
                           gpointer udata)
{
    g_value_set_double(to, 1 - g_value_get_double(from));

    return TRUE;
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
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
realise_cb(GtkWidget* widget,
           GtkWidget* prev_toplvl,
           gpointer udata)
{
    GtPlayerHeaderBar* self = GT_PLAYER_HEADER_BAR(widget);
    GtPlayerHeaderBarPrivate* priv = gt_player_header_bar_get_instance_private(self);
    GtWin* win = GT_WIN_TOPLEVEL(self);

    g_object_bind_property(win, "fullscreen",
                           priv->back_button, "visible",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
    g_object_bind_property(win, "fullscreen",
                           priv->back_separator, "visible",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
    g_object_bind_property(win->player, "volume",
                           priv->volume_button, "value",
                           G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
    g_object_bind_property(win->player, "chat-width",
                           priv->chat_view_width_adjustment, "value",
                           G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
    g_object_bind_property(win->player, "chat-height",
                           priv->chat_view_height_adjustment, "value",
                           G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
    g_object_bind_property(win->player, "chat-x",
                           priv->chat_view_x_adjustment, "value",
                           G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
    g_object_bind_property(win->player, "chat-y",
                           priv->chat_view_y_adjustment, "value",
                           G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
    g_object_bind_property_full(win->player, "chat-width",
                                priv->chat_view_x_adjustment, "upper",
                                G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
                                (GBindingTransformFunc) chat_pos_upper_transformer,
                                NULL, NULL, NULL);
    g_object_bind_property_full(win->player, "chat-height",
                                priv->chat_view_y_adjustment, "upper",
                                G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
                                (GBindingTransformFunc) chat_pos_upper_transformer,
                                NULL, NULL, NULL);
    g_object_bind_property(priv->chat_view_opacity_adjustment, "value",
                           win->player, "chat-opacity",
                           G_BINDING_BIDIRECTIONAL);
    g_object_bind_property(win->player, "chat-visible",
                           priv->edit_chat_button, "visible",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

    g_signal_connect(win->player, "notify::chat-visible", G_CALLBACK(chat_visible_cb), self);
    g_signal_connect(win->player, "notify::channel", G_CALLBACK(player_channel_set_cb), self);
    g_signal_connect(win, "notify::fullscreen", G_CALLBACK(fullscreen_cb), self);

    player_channel_set_cb(NULL, NULL, self);
}

static void
gt_player_header_bar_class_init(GtPlayerHeaderBarClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
                                                "/com/gnome-twitch/ui/gt-player-header-bar.ui");
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, show_chat_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, hide_chat_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, fullscreen_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, fullscreen_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, unfullscreen_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, volume_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, chat_view_opacity_adjustment);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, chat_view_width_adjustment);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, chat_view_height_adjustment);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, chat_view_x_adjustment);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, chat_view_y_adjustment);
//    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, title_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, status_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, name_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, edit_chat_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, dock_chat_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, show_chat_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, back_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtPlayerHeaderBar, back_separator);
}

static void
gt_player_header_bar_init(GtPlayerHeaderBar* self)
{
    GtPlayerHeaderBarPrivate* priv = gt_player_header_bar_get_instance_private(self);

    gtk_widget_init_template(GTK_WIDGET(self));

    gtk_style_context_remove_class(gtk_widget_get_style_context(priv->volume_button), GTK_STYLE_CLASS_FLAT);

    utils_signal_connect_oneshot(self, "realize", G_CALLBACK(realise_cb), NULL);
    g_signal_connect(priv->volume_button, "button-press-event", G_CALLBACK(mute_volume_cb), self);
//    gtk_header_bar_set_custom_title(GTK_HEADER_BAR(self), priv->title_button);
}
