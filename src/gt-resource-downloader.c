#include "gt-resource-downloader.h"
#include "utils.h"
#include "config.h"
#include <glib/gprintf.h>

#define TAG "GtResourceDownloader"
#include "gnome-twitch/gt-log.h"

typedef struct
{
    gchar* filepath;
    SoupSession* soup;
} GtResourceDownloaderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtResourceDownloader, gt_resource_downloader, G_TYPE_OBJECT);

static void
finalize(GObject* obj)
{
    g_assert(GT_IS_RESOURCE_DOWNLOADER(obj));

    GtResourceDownloader* self = GT_RESOURCE_DOWNLOADER(obj);
    GtResourceDownloaderPrivate* priv = gt_resource_downloader_get_instance_private(self);

    g_free(priv->filepath);

    G_OBJECT_CLASS(gt_resource_downloader_parent_class)->finalize(obj);
}

static void
dispose(GObject* obj)
{
    g_assert(GT_IS_RESOURCE_DOWNLOADER(obj));

    GtResourceDownloader* self = GT_RESOURCE_DOWNLOADER(obj);
    GtResourceDownloaderPrivate* priv = gt_resource_downloader_get_instance_private(self);

    g_clear_object(&priv->soup);

    G_OBJECT_CLASS(gt_resource_downloader_parent_class)->dispose(obj);
}

static void
gt_resource_downloader_class_init(GtResourceDownloaderClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = finalize;
    G_OBJECT_CLASS(klass)->dispose = dispose;
}

static void
gt_resource_downloader_init(GtResourceDownloader* self)
{
    g_assert(GT_IS_RESOURCE_DOWNLOADER(self));

    GtResourceDownloaderPrivate* priv = gt_resource_downloader_get_instance_private(self);

    priv->soup = soup_session_new();
}

GtResourceDownloader*
gt_resource_downloader_new(const gchar* filepath)
{
    RETURN_VAL_IF_FAIL(!utils_str_empty(filepath), NULL);

    GtResourceDownloader* ret = g_object_new(GT_TYPE_RESOURCE_DOWNLOADER, NULL);
    GtResourceDownloaderPrivate* priv = gt_resource_downloader_get_instance_private(ret);

    priv->filepath = g_strdup(filepath);

    return ret;
}

GdkPixbuf*
gt_resource_downloader_download_image(GtResourceDownloader* self,
    const gchar* uri, const gchar* name, gboolean cache, GError** error)
{
    RETURN_VAL_IF_FAIL(GT_IS_RESOURCE_DOWNLOADER(self), NULL);
    RETURN_VAL_IF_FAIL(!utils_str_empty(uri), NULL);

    GtResourceDownloaderPrivate* priv = gt_resource_downloader_get_instance_private(self);
    g_autofree gchar* filename = NULL;
    gint64 file_timestamp = 0;
    gboolean file_exists = FALSE;
    g_autoptr(SoupMessage) msg = NULL;
    g_autoptr(GInputStream) istream = NULL;
    GdkPixbuf* ret = NULL;
    GError* err = NULL; /* NOTE: Doesn't need to be freed because we propagate it */

    DEBUG("Downloading image from uri '%s'", uri);

    /* NOTE: If we aren't supplied a filename, we'll just create one by hashing the uri */
    if (utils_str_empty(name))
    {
        gchar hash_str[15];
        guint hash = 0;

        hash = g_str_hash(uri); /* TODO: Replace this with murmur3 hash */

        g_sprintf(hash_str, "%ud", hash);

        filename = g_build_filename(priv->filepath, hash_str, NULL);
    }
    else
        filename = g_build_filename(priv->filepath, name, NULL);

#define CHECK_ERROR                                                     \
    if (err)                                                            \
    {                                                                   \
        WARNING("Could not download image because: %s", err->message);  \
                                                                        \
        g_propagate_prefixed_error(error, err, "Could not download image because: "); \
                                                                        \
        return NULL;                                                    \
    }

    if (cache && (file_exists = g_file_test(filename, G_FILE_TEST_EXISTS)))
    {
        file_timestamp = utils_timestamp_file(filename, &err);

        CHECK_ERROR;
    }

    msg = soup_message_new(SOUP_METHOD_GET, uri);
    soup_message_headers_append(msg->request_headers, "Client-ID", CLIENT_ID);
    istream = soup_session_send(priv->soup, msg, NULL, &err);

    if (!cache) goto download;

    CHECK_ERROR;

    if (SOUP_STATUS_IS_SUCCESSFUL(msg->status_code))
    {
        const gchar* last_modified_str = NULL;

        DEBUG("Successful return code from uri '%s'", uri);

        last_modified_str = soup_message_headers_get_one(msg->response_headers, "Last-Modified");

        if (utils_str_empty(last_modified_str))
        {
            DEBUG("No 'Last-Modified' header in response from uri '%s'", uri);

            goto download;
        }
        else if (utils_http_full_date_to_timestamp(last_modified_str) < file_timestamp)
        {
            DEBUG("No new image at uri '%s'", uri);

            if (file_exists)
            {
                DEBUG("Loading image from file '%s'", filename);

                ret = gdk_pixbuf_new_from_file(filename, &err);

                CHECK_ERROR;
            }
            else
            {
                DEBUG("Image doesn't exist locally");

                goto download;
            }
        }
        else
        {
        download:
            DEBUG("New image at uri '%s'", uri);

            ret = gdk_pixbuf_new_from_stream(istream, NULL, &err);

            CHECK_ERROR;

            gdk_pixbuf_save(ret, filename, "jpeg", &err, "quality", "100", NULL);

            /* NOTE: We won't throw an error here, just log it*/
            if (err)
            {
                WARNING("Unable to save image with filename '%s' because: %s",
                    filename, err->message);

                g_error_free(err);
            }
        }
    }
    else
    {
        WARNING("Received unsuccessful response from uri '%s' with code '%d' and response '%s'",
            uri, msg->status_code, msg->response_body->data);

        g_set_error(error, GT_UTILS_ERROR, GT_UTILS_ERROR_SOUP,
            "Received unsuccessful response from url '%s' with code '%d' and body '%s'",
            uri, msg->status_code, msg->response_body->data);
    }

    return ret;
}


static void
download_image_async_cb(GTask* task, gpointer source,
    gpointer task_data, GCancellable* cancel)
{
    RETURN_IF_FAIL(GT_IS_RESOURCE_DOWNLOADER(source));
    RETURN_IF_FAIL(G_IS_TASK(task));
    RETURN_IF_FAIL(task_data != NULL);

    GtResourceDownloader* self = GT_RESOURCE_DOWNLOADER(source);
    GError* err = NULL;
    GenericTaskData* data = task_data;

    GdkPixbuf* ret = gt_resource_downloader_download_image(self,
        data->str_1, data->str_2, data->bool_1, &err);

    if (err)
        g_task_return_error(task, err);
    else
        g_task_return_pointer(task, ret, (GDestroyNotify) g_object_unref);
}

void
gt_resource_downloader_download_image_async(GtResourceDownloader* self,
    const gchar* uri, const gchar* name, gboolean cache, GAsyncReadyCallback cb,
    GCancellable* cancel, gpointer udata)
{
    RETURN_IF_FAIL(GT_IS_RESOURCE_DOWNLOADER(source));

    g_autoptr(GTask) task = g_task_new(self, cancel, cb, udata);
    GenericTaskData* data = generic_task_data_new();

    data->str_1 = g_strdup(uri);
    data->str_2 = g_strdup(name);
    data->bool_1 = cache;

    g_task_set_task_data(task, data, (GDestroyNotify) generic_task_data_free);

    g_task_run_in_thread(task, download_image_async_cb);
}

GdkPixbuf*
gt_resource_donwloader_download_image_finish(GtResourceDownloader* self,
    GAsyncResult* result, GError** error)
{
    RETURN_VAL_IF_FAIL(GT_IS_RESOURCE_DONWLOADER(self), NULL);
    RETURN_VAL_IF_FAIL(G_IS_ASYNC_RESULT(result), NULL);

    GdkPixbuf* ret = g_task_propagate_pointer(G_TASK(result), error);

    return ret;
}
