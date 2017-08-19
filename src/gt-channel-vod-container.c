#include "gt-channel-vod-container.h"
#include "gt-app.h"
#include "gt-vod.h"
#include "gt-vod-container-child.h"
#include "gt-win.h"
#include "utils.h"
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#define TAG "GtChannelVODContainer"
#include "gnome-twitch/gt-log.h"

#define CHILD_WIDTH 350
#define CHILD_HEIGHT 230
#define APPEND_EXTRA TRUE

typedef struct
{
    gchar* channel_id;
    JsonParser* json_parser;
    GCancellable* cancel;
} GtChannelVODContainerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtChannelVODContainer, gt_channel_vod_container, GT_TYPE_ITEM_CONTAINER);

enum
{
    PROP_0,
    PROP_CHANNEL_ID,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

static void
get_container_properties(GtItemContainer* self, GtItemContainerProperties* props)
{
    props->child_width = CHILD_WIDTH;
    props->child_height = CHILD_HEIGHT;
    props->append_extra = APPEND_EXTRA;
    props->empty_label_text = g_strdup(_("No VODs found"));
    props->empty_sub_label_text = g_strdup(_("This channel hasn't published any VODs"));
    props->error_label_text = g_strdup(_("An error occurred when fetching VODs"));
    props->empty_image_name = g_strdup("face-plain-symbolic");
    props->fetching_label_text = g_strdup(_("Fetching VODs"));
}

static GtkWidget*
create_child(GtItemContainer* item_container, gpointer data)
{
    RETURN_VAL_IF_FAIL(GT_IS_CHANNEL_VOD_CONTAINER(item_container), NULL);
    RETURN_VAL_IF_FAIL(GT_IS_VOD(data), NULL);

    return GTK_WIDGET(gt_vod_container_child_new(GT_VOD(data)));
}

static void
activate_child(GtItemContainer* item_container, gpointer child)
{
    RETURN_IF_FAIL(GT_IS_CHANNEL_VOD_CONTAINER(item_container));
    RETURN_IF_FAIL(GT_IS_VOD_CONTAINER_CHILD(child));

    GtChannelVODContainer* self = GT_CHANNEL_VOD_CONTAINER(item_container);
    GtChannelVODContainerPrivate* priv = gt_channel_vod_container_get_instance_private(self);

    GtWin* win = GT_WIN_TOPLEVEL(self);

    RETURN_IF_FAIL(GT_IS_WIN(win));

    gt_win_play_vod(win, GT_VOD_CONTAINER_CHILD(child)->vod);
}

static void
process_json_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    RETURN_IF_FAIL(JSON_IS_PARSER(source));
    RETURN_IF_FAIL(G_IS_ASYNC_RESULT(res));
    RETURN_IF_FAIL(udata != NULL);

    g_autoptr(GWeakRef) ref = udata;
    g_autoptr(GtChannelVODContainer) self = g_weak_ref_get(ref);

    if (!self)
    {
        TRACE("Not processing json because we were unreffed while waiting");
        return;
    }

    GtChannelVODContainerPrivate* priv = gt_channel_vod_container_get_instance_private(self);
    g_autoptr(JsonReader) reader = NULL;
    g_autoptr(GtGameList) items = NULL;
    g_autoptr(GError) err = NULL;

    json_parser_load_from_stream_finish(priv->json_parser, res, &err);

    if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        DEBUG("Cancelled");
        return;
    }
    else if (err)
    {
        WARNING("Unable to process JSON because: %s", err->message);
        gt_item_container_show_error(GT_ITEM_CONTAINER(self), err);
        return;
    }

    reader = json_reader_new(json_parser_get_root(priv->json_parser));

    if (!json_reader_read_member(reader, "videos"))
    {
        WARNING("Unable to process JSON because: %s",
            json_reader_get_error(reader)->message);
        gt_item_container_show_error(GT_ITEM_CONTAINER(self),
            json_reader_get_error(reader));
        return;
    }

    for (gint i = 0; i < json_reader_count_elements(reader); i++)
    {
        g_autoptr(GtVODData) data = NULL;

        if (!json_reader_read_element(reader, i))
        {
            WARNING("Unable to process JSON because: %s",
                json_reader_get_error(reader)->message);
            gt_item_container_show_error(GT_ITEM_CONTAINER(self),
                json_reader_get_error(reader));
            return;
        }

        data = utils_parse_vod_from_json(reader, &err);

        if (err)
        {
            WARNING("Unable to process JSON because: %s", err->message);
            gt_item_container_show_error(GT_ITEM_CONTAINER(self), err);
            return;
        }

        items = g_list_append(items, gt_vod_new(g_steal_pointer(&data)));

        json_reader_end_element(reader);
    }

    json_reader_end_member(reader);

    gt_item_container_append_items(GT_ITEM_CONTAINER(self), g_steal_pointer(&items));
    gt_item_container_set_fetching_items(GT_ITEM_CONTAINER(self), FALSE);
}


static void
handle_response_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    RETURN_IF_FAIL(SOUP_IS_SESSION(source));
    RETURN_IF_FAIL(G_IS_ASYNC_RESULT(res));
    RETURN_IF_FAIL(udata != NULL);

    g_autoptr(GWeakRef) ref = udata;
    g_autoptr(GtChannelVODContainer) self = g_weak_ref_get(ref);

    if (!self)
    {
        TRACE("Not handling response because we were unreffed while waiting");
        return;
    }

    GtChannelVODContainerPrivate* priv = gt_channel_vod_container_get_instance_private(self);
    g_autoptr(GInputStream) istream = NULL;
    g_autoptr(GError) err = NULL;

    istream = soup_session_send_finish(main_app->soup, res, &err);

    if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        DEBUG("Cancelled");
        return;
    }
    else if (err)
    {
        WARNING("Unable to handle response because: %s", err->message);
        gt_item_container_show_error(GT_ITEM_CONTAINER(self), err);
        return;
    }

    json_parser_load_from_stream_async(priv->json_parser, istream,
        priv->cancel, process_json_cb, g_steal_pointer(&ref));
}

static void
request_extra_items(GtItemContainer* item_container,
    gint amount, gint offset)
{
    RETURN_IF_FAIL(GT_IS_CHANNEL_VOD_CONTAINER(item_container));
    RETURN_IF_FAIL(amount > 0);
    RETURN_IF_FAIL(amount <= 100);
    RETURN_IF_FAIL(offset >= 0);

    GtChannelVODContainer* self = GT_CHANNEL_VOD_CONTAINER(item_container);
    GtChannelVODContainerPrivate* priv = gt_channel_vod_container_get_instance_private(self);
    g_autoptr(SoupMessage) msg = NULL;

    utils_refresh_cancellable(&priv->cancel);

    msg = utils_create_twitch_request_v("https://api.twitch.tv/kraken/channels/%s/videos?limit=%d&offset=%d",
        priv->channel_id, amount, offset);

    gt_app_queue_soup_message(main_app, "item-container", msg,
        priv->cancel, handle_response_cb, utils_weak_ref_new(self));

}

static void
get_property(GObject* obj, guint prop,
    GValue* val, GParamSpec* pspec)
{
    GtChannelVODContainer* self = GT_CHANNEL_VOD_CONTAINER(obj);
    GtChannelVODContainerPrivate* priv = gt_channel_vod_container_get_instance_private(self);

    switch (prop)
    {
        case PROP_CHANNEL_ID:
            g_value_set_string(val, priv->channel_id);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
set_property(GObject* obj, guint prop,
    const GValue* val, GParamSpec* pspec)
{
    GtChannelVODContainer* self = GT_CHANNEL_VOD_CONTAINER(obj);
    GtChannelVODContainerPrivate* priv = gt_channel_vod_container_get_instance_private(self);

    switch (prop)
    {
        case PROP_CHANNEL_ID:
            g_free(priv->channel_id);
            priv->channel_id = g_value_dup_string(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_channel_vod_container_class_init(GtChannelVODContainerClass* klass)
{
    G_OBJECT_CLASS(klass)->get_property = get_property;
    G_OBJECT_CLASS(klass)->set_property = set_property;

    GT_ITEM_CONTAINER_CLASS(klass)->get_container_properties = get_container_properties;
    GT_ITEM_CONTAINER_CLASS(klass)->request_extra_items = request_extra_items;
    GT_ITEM_CONTAINER_CLASS(klass)->create_child = create_child;
    GT_ITEM_CONTAINER_CLASS(klass)->activate_child = activate_child;

    props[PROP_CHANNEL_ID] = g_param_spec_string("channel-id", "Channel ID", "ID of the currently open channel",
            NULL, G_PARAM_READWRITE);

    g_object_class_install_properties(G_OBJECT_CLASS(klass), NUM_PROPS, props);
}

static void
gt_channel_vod_container_init(GtChannelVODContainer* self)
{
    GtChannelVODContainerPrivate* priv = gt_channel_vod_container_get_instance_private(self);

    priv->channel_id = NULL;
    priv->cancel = NULL;
    priv->json_parser = json_parser_new();
}
