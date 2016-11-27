#include <glib/gi18n.h>
#include "gt-top-channel-container.h"
#include "gt-twitch.h"
#include "gt-app.h"
#include "gt-win.h"
#include "gt-channels-container-child.h"
#include "gt-channel.h"

#define TAG "GtTopChannelContainer"
#include "gnome-twitch/gt-log.h"

#define CHILD_WIDTH 350
#define CHILD_HEIGHT 230
#define APPEND_EXTRA TRUE

typedef struct
{
    gpointer tmp;
} GtTopChannelContainerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtTopChannelContainer, gt_top_channel_container, GT_TYPE_ITEM_CONTAINER);

static void
get_properties(GtItemContainer* self,
    gint* child_width, gint* child_height, gboolean* append_extra,
    gchar** empty_label_text, gchar** empty_sub_label_text, gchar** empty_image_name, gchar** fetching_label_text)
{
    *child_width = CHILD_WIDTH;
    *child_height = CHILD_HEIGHT;
    *append_extra = APPEND_EXTRA;
    *empty_label_text = g_strdup(_("No channels found"));
    *empty_sub_label_text = g_strdup(_("Probably an error occurred, try refreshing"));
    *empty_image_name = g_strdup("view-refresh-symbolic");
    *fetching_label_text = g_strdup(_("Fetching channels"));
}

static void
fetch_items(GTask* task,
    gpointer source, gpointer task_data,
    GCancellable* cancel)
{
    if (g_task_return_error_if_cancelled(task))
        return;

    FetchItemsData* data = task_data;

    //TODO: Check for error
    GList* items = gt_twitch_top_channels(main_app->twitch,
        data->amount, data->offset, NO_GAME);

    g_task_return_pointer(task, items, (GDestroyNotify) gt_channel_list_free);
}

static GtkWidget*
create_child(gpointer data)
{
    return GTK_WIDGET(gt_channels_container_child_new(GT_CHANNEL(data)));
}

static void
activate_child(GtItemContainer* item_container,
    gpointer child)
{
    gt_win_open_channel(GT_WIN_ACTIVE,
        GT_CHANNELS_CONTAINER_CHILD(child)->channel);
}

static void
gt_top_channel_container_class_init(GtTopChannelContainerClass* klass)
{
    GT_ITEM_CONTAINER_CLASS(klass)->create_child = create_child;
    GT_ITEM_CONTAINER_CLASS(klass)->get_properties = get_properties;
    GT_ITEM_CONTAINER_CLASS(klass)->fetch_items = fetch_items;
    GT_ITEM_CONTAINER_CLASS(klass)->activate_child = activate_child;
}

static void
gt_top_channel_container_init(GtTopChannelContainer* self)
{
}

GtTopChannelContainer*
gt_top_channel_container_new()
{
    return g_object_new(GT_TYPE_TOP_CHANNEL_CONTAINER,
        NULL);
}
