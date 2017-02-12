#include "gt-channels-container-child.h"
#include "gt-win.h"
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#define TAG "GtChannelsContainerChild"
#include "utils.h"

typedef struct
{
    GtkWidget* preview_image;
    GtkWidget* name_label;
    GtkWidget* game_label;
    GtkWidget* preview_box;
    GtkWidget* preview_overlay_revealer;
    GtkWidget* viewers_label;
    GtkWidget* viewers_image;
    GtkWidget* time_label;
    GtkWidget* time_image;
    GtkWidget* follow_button;
    GtkWidget* preview_stack;
    GtkWidget* play_image;
    GtkWidget* error_box;
    GtkWidget* error_label;
    GtkWidget* error_reload_button;
    GtkWidget* error_link_button;
    GtkWidget* updating_spinner;
} GtChannelsContainerChildPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtChannelsContainerChild, gt_channels_container_child, GTK_TYPE_FLOW_BOX_CHILD)

enum
{
    PROP_0,
    PROP_CHANNEL,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtChannelsContainerChild*
gt_channels_container_child_new(GtChannel* chan)
{
    return g_object_new(GT_TYPE_CHANNELS_CONTAINER_CHILD,
                        "channel", chan,
                        NULL);
}

static void
motion_enter_cb(GtkWidget* widget,
                GdkEvent* evt,
                gpointer udata)
{
    GtChannelsContainerChild* self = GT_CHANNELS_CONTAINER_CHILD(udata);
    GtChannelsContainerChildPrivate* priv = gt_channels_container_child_get_instance_private(self);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->preview_overlay_revealer), TRUE);
}

static void
motion_leave_cb(GtkWidget* widget,
                GdkEvent* evt,
                gpointer udata)
{
    GtChannelsContainerChild* self = GT_CHANNELS_CONTAINER_CHILD(udata);
    GtChannelsContainerChildPrivate* priv = gt_channels_container_child_get_instance_private(self);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->preview_overlay_revealer), FALSE);
}

static void
follow_button_cb(GtkButton* button,
                    gpointer udata)
{
    GtChannelsContainerChild* self = GT_CHANNELS_CONTAINER_CHILD(udata);

    gt_channel_toggle_followed(self->channel);
}

static gboolean
viewers_converter(GBinding* bind,
                  const GValue* from,
                  GValue* to,
                  gpointer udata)
{
    gint64 viewers;
    gchar* label = NULL;

    if (g_value_get_int64(from) > -1)
    {
        viewers = g_value_get_int64(from);

        if (viewers > 1e4)
            // Translators: Used for when viewers >= 1000
            // Shorthand for thousands. Ex (English): 6200 = 6.2k
            label = g_strdup_printf(_("%3.1fk"), (gdouble) viewers / 1e3);
        else
            // Translators: Used for when viewers < 1000
            // No need to translate, just future-proofing
            label = g_strdup_printf(_("%ld"), viewers);
    }

    g_value_take_string(to, label);

    return TRUE;
}

static gboolean
time_converter(GBinding* bind,
               const GValue* from,
               GValue* to,
               gpointer udata)
{
    gchar* label = NULL;
    GDateTime* now_time;
    GDateTime* stream_started_time;
    GTimeSpan dif;

    if (g_value_get_pointer(from) != NULL)
    {
        now_time = g_date_time_new_now_utc();
        stream_started_time = (GDateTime*) g_value_get_pointer(from);

        dif = g_date_time_difference(now_time, stream_started_time);

        if (dif > G_TIME_SPAN_HOUR)
            // Translators: Used for when stream time > 60 min
            // Ex (English): 3 hours and 45 minutes = 3.75h
            label = g_strdup_printf(_("%2.1fh"), (gdouble) dif / G_TIME_SPAN_HOUR);
        else
            // Translators: Used when stream time <= 60min
            // Ex (English): 45 minutes = 45m
            label  = g_strdup_printf(_("%ldm"), dif / G_TIME_SPAN_MINUTE);

        g_date_time_unref(now_time);
    }

    g_value_take_string(to, label);

    return TRUE;
}

static void
error_link_clicked_cb(GtkButton* button,
    gpointer udata)
{
    g_assert(GT_IS_CHANNELS_CONTAINER_CHILD(udata));

    GtChannelsContainerChild* self = GT_CHANNELS_CONTAINER_CHILD(udata);

    GtWin* win = GT_WIN_TOPLEVEL(self);

    g_assert(GT_IS_WIN(win));

    gt_win_show_error_dialogue(win, gt_channel_get_error_message(self->channel),
        gt_channel_get_error_details(self->channel));
}

static void
online_cb(GtChannelsContainerChild* self)
{
    g_assert(GT_IS_CHANNELS_CONTAINER_CHILD(self));

    if (gt_channel_is_online(self->channel))
        REMOVE_STYLE_CLASS(self, "gt-channels-container-child-offline");
    else
        ADD_STYLE_CLASS(self, "gt-channels-container-child-offline");
}

static void
state_changed_cb(GtChannelsContainerChild* self)
{
    g_assert(GT_IS_CHANNELS_CONTAINER_CHILD(self));

    GtChannelsContainerChildPrivate* priv = gt_channels_container_child_get_instance_private(self);

    if (gt_channel_is_error(self->channel))
        gtk_stack_set_visible_child(GTK_STACK(priv->preview_stack), priv->error_box);
    else if (gt_channel_is_updating(self->channel))
        gtk_stack_set_visible_child(GTK_STACK(priv->preview_stack), priv->updating_spinner);
    else
        gtk_stack_set_visible_child(GTK_STACK(priv->preview_stack), priv->preview_box);
}

static void
dispose(GObject* object)
{
    GtChannelsContainerChild* self = GT_CHANNELS_CONTAINER_CHILD(object);

    g_clear_object(&self->channel);

    G_OBJECT_CLASS(gt_channels_container_child_parent_class)->dispose(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtChannelsContainerChild* self = GT_CHANNELS_CONTAINER_CHILD(obj);
    GtChannelsContainerChildPrivate* priv = gt_channels_container_child_get_instance_private(self);

    switch (prop)
    {
        case PROP_CHANNEL:
            g_value_set_object(val, self->channel);
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
    GtChannelsContainerChild* self = GT_CHANNELS_CONTAINER_CHILD(obj);
    GtChannelsContainerChildPrivate* priv = gt_channels_container_child_get_instance_private(self);

    switch (prop)
    {
        case PROP_CHANNEL:
            g_clear_object(&self->channel);
            self->channel = utils_value_ref_sink_object(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
constructed(GObject* obj)
{
    GtChannelsContainerChild* self = GT_CHANNELS_CONTAINER_CHILD(obj);
    GtChannelsContainerChildPrivate* priv = gt_channels_container_child_get_instance_private(self);

    g_object_bind_property(self->channel, "display-name",
                           priv->name_label, "label",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(self->channel, "game",
                           priv->game_label, "label",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(self->channel, "followed",
                           priv->follow_button, "active",
                           G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
    g_object_bind_property(self->channel, "preview",
                           priv->preview_image, "pixbuf",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property_full(self->channel, "viewers",
                                priv->viewers_label, "label",
                                G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
                                (GBindingTransformFunc) viewers_converter,
                                NULL, NULL, NULL);
    g_object_bind_property_full(self->channel, "stream-started-time",
                                priv->time_label, "label",
                                G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
                                (GBindingTransformFunc) time_converter,
                                NULL, NULL, NULL);
    g_object_bind_property(self->channel, "online",
                           priv->viewers_label, "visible",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(self->channel, "online",
                           priv->viewers_image, "visible",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(self->channel, "online",
                           priv->time_label, "visible",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(self->channel, "online",
                           priv->time_image, "visible",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property(self->channel, "online",
                           priv->play_image, "visible",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

    g_signal_connect_swapped(self->channel, "notify::online", G_CALLBACK(online_cb), self);
    g_signal_connect_swapped(self->channel, "notify::error", G_CALLBACK(state_changed_cb), self);
    g_signal_connect_swapped(self->channel, "notify::updating", G_CALLBACK(state_changed_cb), self);
    g_signal_connect_swapped(priv->error_reload_button, "clicked", G_CALLBACK(gt_channel_update), self->channel);

    state_changed_cb(self);
    online_cb(self);

    G_OBJECT_CLASS(gt_channels_container_child_parent_class)->constructed(obj);
}

static void
gt_channels_container_child_class_init(GtChannelsContainerChildClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = dispose;
    object_class->get_property = get_property;
    object_class->set_property = set_property;
    object_class->constructed = constructed;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
        "/com/vinszent/GnomeTwitch/ui/gt-channels-container-child.ui");

    props[PROP_CHANNEL] = g_param_spec_object("channel", "Channel", "Associated channel",
        GT_TYPE_CHANNEL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties(object_class, NUM_PROPS, props);

    //TODO: Move these binds into code?
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), motion_enter_cb);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), motion_leave_cb);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), follow_button_cb);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, preview_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, name_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, game_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, preview_overlay_revealer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, preview_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, viewers_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, viewers_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, time_label);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, time_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, follow_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, preview_stack);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, play_image);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, error_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, error_link_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, error_reload_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtChannelsContainerChild, updating_spinner);
}

static void
gt_channels_container_child_init(GtChannelsContainerChild* self)
{
    GtChannelsContainerChildPrivate* priv = gt_channels_container_child_get_instance_private(self);

    gtk_widget_init_template(GTK_WIDGET(self));

    g_signal_connect(priv->error_link_button, "clicked", G_CALLBACK(error_link_clicked_cb), self);
}

void
gt_channels_container_child_hide_overlay(GtChannelsContainerChild* self)
{
    GtChannelsContainerChildPrivate* priv = gt_channels_container_child_get_instance_private(self);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->preview_overlay_revealer), FALSE);
}
