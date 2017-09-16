#include "gt-channel-header-bar.h"
#include "gt-win.h"
#include "gt-channel.h"
#include "utils.h"

#define TAG "GtChannelHeaderBar"
#include "gnome-twitch/gt-log.h"

typedef struct
{
    GtkWidget* status_label;
    GtkWidget* name_label;
} GtChannelHeaderBarPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtChannelHeaderBar, gt_channel_header_bar, GTK_TYPE_HEADER_BAR);

static void
channel_opened_cb(GObject* source, GParamSpec* pspec, gpointer udata)
{
    RETURN_IF_FAIL(GT_IS_WIN(source));
    RETURN_IF_FAIL(G_IS_PARAM_SPEC(pspec));
    RETURN_IF_FAIL(GT_IS_CHANNEL_HEADER_BAR(udata));

    GtChannelHeaderBar* self = GT_CHANNEL_HEADER_BAR(udata);
    GtChannelHeaderBarPrivate* priv = gt_channel_header_bar_get_instance_private(self);
    GtWin* win = GT_WIN(source);

    gtk_label_set_label(GTK_LABEL(priv->status_label),
        gt_channel_get_status(win->open_channel));

    gtk_label_set_label(GTK_LABEL(priv->name_label),
        gt_channel_get_display_name(win->open_channel));
}

static void
realize_cb(GtkWidget* widget, gpointer udata)
{
    g_assert(GT_IS_CHANNEL_HEADER_BAR(widget));

    GtChannelHeaderBar* self = GT_CHANNEL_HEADER_BAR(widget);
    GtWin* win = GT_WIN_TOPLEVEL(self);

    RETURN_IF_FAIL(GT_IS_WIN(win));

    g_signal_connect_object(win, "notify:open-channel", G_CALLBACK(channel_opened_cb), self, 0);
}

static void
gt_channel_header_bar_class_init(GtChannelHeaderBarClass* klass)
{
    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
        "/com/vinszent/GnomeTwitch/ui/gt-channel-header-bar.ui");
}

static void
gt_channel_header_bar_init(GtChannelHeaderBar* self)
{
    gtk_widget_init_template(GTK_WIDGET(self));

    utils_signal_connect_oneshot(self, "realize", G_CALLBACK(realize_cb), NULL);
}
