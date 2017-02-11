#include <glib/gi18n.h>
#include "gt-game-channel-container.h"
#include "gt-twitch.h"
#include "gt-app.h"
#include "gt-win.h"
#include "gt-channels-container-child.h"
#include "gt-channel.h"

#define CHILD_WIDTH 350
#define CHILD_HEIGHT 230
#define APPEND_EXTRA TRUE

typedef struct
{
    gchar* game;
} GtGameChannelContainerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtGameChannelContainer, gt_game_channel_container, GT_TYPE_ITEM_CONTAINER);
enum
{
    PROP_0,
    PROP_GAME,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

static void
get_properties(GtItemContainer* self,
    gint* child_width, gint* child_height, gboolean* append_extra,
    gchar** empty_label_text, gchar** empty_sub_label_text, gchar** empty_image_name,
    gchar** error_label_text, gchar** fetching_label_text)
{
    *child_width = CHILD_WIDTH;
    *child_height = CHILD_HEIGHT;
    *append_extra = APPEND_EXTRA;
    *empty_label_text = g_strdup(_("No channels found"));
    *empty_sub_label_text = g_strdup(_("Nobody is currently streaming this game"));
    *error_label_text = g_strdup(_("An error occurred when fetching the channels"));
    *empty_image_name = g_strdup("face-plain-symbolic");
    *fetching_label_text = g_strdup("Fetching channels");
}

static void
fetch_items(GTask* task,
    gpointer source, gpointer task_data,
    GCancellable* cancel)
{
    g_assert(G_IS_TASK(task));

    if (g_task_return_error_if_cancelled(task))
        return;

    g_assert(GT_IS_GAME_CHANNEL_CONTAINER(source));

    GtGameChannelContainer* self = GT_GAME_CHANNEL_CONTAINER(source);
    GtGameChannelContainerPrivate* priv = gt_game_channel_container_get_instance_private(self);

    FetchItemsData* data = task_data;
    GError* err = NULL;

    g_assert_nonnull(data);

    //TODO: Check for error
    GList* items = gt_twitch_top_channels(main_app->twitch,
        data->amount, data->offset, priv->game, &err);

    if (err)
        g_task_return_error(task, err);
    else
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
get_property(GObject* obj,
    guint prop,
    GValue* val,
    GParamSpec* pspec)
{
    GtGameChannelContainer* self = GT_GAME_CHANNEL_CONTAINER(obj);
    GtGameChannelContainerPrivate* priv = gt_game_channel_container_get_instance_private(self);

    switch (prop)
    {
        case PROP_GAME:
            g_value_set_string(val, priv->game);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
set_property(GObject* obj,
    guint prop,
    const GValue* val,
    GParamSpec* pspec)
{
    GtGameChannelContainer* self = GT_GAME_CHANNEL_CONTAINER(obj);
    GtGameChannelContainerPrivate* priv = gt_game_channel_container_get_instance_private(self);

    switch (prop)
    {
        case PROP_GAME:
            g_clear_pointer(&priv->game, g_free);
            priv->game = g_value_dup_string(val);

            gt_item_container_refresh(GT_ITEM_CONTAINER(self));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_game_channel_container_class_init(GtGameChannelContainerClass* klass)
{
    G_OBJECT_CLASS(klass)->get_property = get_property;
    G_OBJECT_CLASS(klass)->set_property = set_property;

    GT_ITEM_CONTAINER_CLASS(klass)->create_child = create_child;
    GT_ITEM_CONTAINER_CLASS(klass)->get_properties = get_properties;
    GT_ITEM_CONTAINER_CLASS(klass)->fetch_items = fetch_items;
    GT_ITEM_CONTAINER_CLASS(klass)->activate_child = activate_child;

    props[PROP_GAME] = g_param_spec_string("game", "Game", "Current game", NULL, G_PARAM_READWRITE);

    g_object_class_install_properties(G_OBJECT_CLASS(klass), NUM_PROPS, props);
}

static void
gt_game_channel_container_init(GtGameChannelContainer* self)
{

}

GtGameChannelContainer*
gt_game_channel_container_new()
{
    return g_object_new(GT_TYPE_GAME_CHANNEL_CONTAINER,
        NULL);
}
