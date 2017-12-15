#include "gt-resource-downloader.h"
#include "utils.h"
#include "config.h"
#include <glib/gprintf.h>

#define TAG "GtResourceDownloader"
#include "gnome-twitch/gt-log.h"

typedef struct
{
    gchar* filepath;
    gchar* image_filetype;
    SoupSession* soup;
    GMutex mutex;
} GtResourceDownloaderPrivate;

typedef struct
{
    gchar* uri;
    gchar* name;
    ResourceDownloaderFunc cb;
    gpointer udata;
    GtResourceDownloader* self;
    SoupMessage* msg;
    GInputStream* istream;
} ResourceData; /* FIXME: Better name? */

static GThreadPool* dl_pool;

G_DEFINE_TYPE_WITH_PRIVATE(GtResourceDownloader, gt_resource_downloader, G_TYPE_OBJECT);

static ResourceData*
resource_data_new()
{
    return g_slice_new0(ResourceData);
}

static void
resource_data_free(ResourceData* data)
{
    if (!data) return;

    g_free(data->uri);
    g_free(data->name);
    g_object_unref(data->self);
    g_object_unref(data->msg);
    g_object_unref(data->istream);

    g_slice_free(ResourceData, data);
}

/* FIXME: Throw error */
static GdkPixbuf*
download_image(GtResourceDownloader* self,
    const gchar* uri, const gchar* name,
    SoupMessage* msg, GInputStream* istream,
    gboolean* from_file, GError** error)
{
    RETURN_VAL_IF_FAIL(GT_IS_RESOURCE_DOWNLOADER(self), NULL);
    RETURN_VAL_IF_FAIL(!utils_str_empty(uri), NULL);
    RETURN_VAL_IF_FAIL(SOUP_IS_MESSAGE(msg), NULL);
    RETURN_VAL_IF_FAIL(G_IS_INPUT_STREAM(istream), NULL);

    GtResourceDownloaderPrivate* priv = gt_resource_downloader_get_instance_private(self);
    g_autofree gchar* filename = NULL;
    gint64 file_timestamp = 0;
    gboolean file_exists = FALSE;
    g_autoptr(GdkPixbuf) ret = NULL;
    g_autoptr(GError) err = NULL;

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

    if (priv->filepath && (file_exists = g_file_test(filename, G_FILE_TEST_EXISTS)))
    {
        file_timestamp = utils_timestamp_filename(filename, NULL);
    }

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

                ret = gdk_pixbuf_new_from_file(filename, NULL);

                if (from_file) *from_file = TRUE;
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

            if (err)
            {
                WARNING("Unable to download image from uri '%s' because: %s",
                    uri, err->message);

                g_propagate_prefixed_error(error, g_steal_pointer(&err),
                    "Unable to download image from uri '%s' because: ", uri);

                return NULL;
            }

            if (priv->filepath && STRING_EQUALS(priv->image_filetype, GT_IMAGE_FILETYPE_JPEG))
            {
                gdk_pixbuf_save(ret, filename, priv->image_filetype,
                    NULL, "quality", "100", NULL);
            }
            else if (priv->filepath)
            {
                gdk_pixbuf_save(ret, filename, priv->image_filetype,
                    NULL, NULL);
            }

            if (from_file) *from_file = FALSE;
        }
    }

    return g_steal_pointer(&ret);
}

static void
download_cb(ResourceData* data,
    gpointer udata)
{
    RETURN_IF_FAIL(data != NULL);

    g_autoptr(GdkPixbuf) ret = NULL;
    g_autoptr(GError) err = NULL;
    gboolean from_file = FALSE;

    ret = download_image(data->self, data->uri, data->name, data->msg,
        data->istream, &from_file, &err);

    data->cb(from_file ? NULL : g_steal_pointer(&ret),
        data->udata, g_steal_pointer(&err));

    resource_data_free(data);
}

static void
send_message_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    RETURN_IF_FAIL(SOUP_IS_SESSION(source));
    RETURN_IF_FAIL(G_IS_ASYNC_RESULT(res));
    RETURN_IF_FAIL(udata != NULL);

    ResourceData* data = udata;
    GtResourceDownloader* self = GT_RESOURCE_DOWNLOADER(data->self);
    GtResourceDownloaderPrivate* priv = gt_resource_downloader_get_instance_private(self);
    g_autoptr(GError) err = NULL;

    data->istream = soup_session_send_finish(SOUP_SESSION(source), res, &err);

    if (!err)
        g_thread_pool_push(dl_pool, data, NULL);
    else
    {
        data->cb(NULL, data->udata, g_steal_pointer(&err));
        resource_data_free(data);
    }
}

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

    MESSAGE("Finalize");

    g_clear_object(&priv->soup);

    G_OBJECT_CLASS(gt_resource_downloader_parent_class)->dispose(obj);
}

static void
gt_resource_downloader_class_init(GtResourceDownloaderClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = finalize;
    G_OBJECT_CLASS(klass)->dispose = dispose;

    dl_pool = g_thread_pool_new((GFunc) download_cb, NULL, g_get_num_processors(), FALSE, NULL);
}

static void
gt_resource_downloader_init(GtResourceDownloader* self)
{
    g_assert(GT_IS_RESOURCE_DOWNLOADER(self));

    GtResourceDownloaderPrivate* priv = gt_resource_downloader_get_instance_private(self);

    priv->soup = soup_session_new();

    g_mutex_init(&priv->mutex);
}

GtResourceDownloader*
gt_resource_downloader_new()
{
    GtResourceDownloader* ret = g_object_new(GT_TYPE_RESOURCE_DOWNLOADER, NULL);

    return ret;
}


GtResourceDownloader*
gt_resource_downloader_new_with_cache(const gchar* filepath)
{
    RETURN_VAL_IF_FAIL(!utils_str_empty(filepath), NULL);

    GtResourceDownloader* ret = g_object_new(GT_TYPE_RESOURCE_DOWNLOADER, NULL);
    GtResourceDownloaderPrivate* priv = gt_resource_downloader_get_instance_private(ret);

    priv->filepath = g_strdup(filepath);
    priv->image_filetype = NULL;

    return ret;
}

GdkPixbuf*
gt_resource_downloader_download_image(GtResourceDownloader* self,
    const gchar* uri, const gchar* name, GError** error)
{
    RETURN_VAL_IF_FAIL(GT_IS_RESOURCE_DOWNLOADER(self), NULL);
    RETURN_VAL_IF_FAIL(!utils_str_empty(uri), NULL);

    GtResourceDownloaderPrivate* priv = gt_resource_downloader_get_instance_private(self);
    g_autoptr(SoupMessage) msg = NULL;
    g_autoptr(GInputStream) istream = NULL;
    g_autoptr(GdkPixbuf) ret = NULL;
    g_autoptr(GError) err = NULL;

    DEBUG("Downloading image from uri '%s'", uri);

    msg = soup_message_new(SOUP_METHOD_GET, uri);

    /* NOTE: So libsoup isn't actually all that thread safe and
     * calling soup_session_send from multiple threads causes it to
     * crash, so we wrap a mutex around it. One should use the
     * download_image_immediately func if one wants to download
     * several images at the same time */
    g_mutex_lock(&priv->mutex);
    istream = soup_session_send(priv->soup, msg, NULL, &err);
    g_mutex_unlock(&priv->mutex);

    if (err)
    {
        WARNING("Unable to download image from uri '%s' because: %s",
            uri, err->message);

        g_propagate_prefixed_error(error, g_steal_pointer(&err),
            "Unable to download image from uri '%s' because: ", uri);

        return NULL;
    }

    ret = download_image(self, uri, name, msg, istream, NULL, error);

    return g_steal_pointer(&ret);
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
        data->str_1, data->str_2, &err);

    if (err)
        g_task_return_error(task, err);
    else
        g_task_return_pointer(task, ret, (GDestroyNotify) g_object_unref);
}

void
gt_resource_downloader_download_image_async(GtResourceDownloader* self,
    const gchar* uri, const gchar* name, GAsyncReadyCallback cb,
    GCancellable* cancel, gpointer udata)
{
    RETURN_IF_FAIL(GT_IS_RESOURCE_DOWNLOADER(self));

    g_autoptr(GTask) task = g_task_new(self, cancel, cb, udata);
    GenericTaskData* data = generic_task_data_new();

    data->str_1 = g_strdup(uri);
    data->str_2 = g_strdup(name);

    g_task_set_task_data(task, data, (GDestroyNotify) generic_task_data_free);

    g_task_run_in_thread(task, download_image_async_cb);
}

GdkPixbuf*
gt_resource_donwloader_download_image_finish(GtResourceDownloader* self,
    GAsyncResult* result, GError** error)
{
    RETURN_VAL_IF_FAIL(GT_IS_RESOURCE_DOWNLOADER(self), NULL);
    RETURN_VAL_IF_FAIL(G_IS_ASYNC_RESULT(result), NULL);

    GdkPixbuf* ret = g_task_propagate_pointer(G_TASK(result), error);

    return ret;
}

void
gt_resource_downloader_set_image_filetype(GtResourceDownloader* self, const gchar* image_filetype)
{
    RETURN_IF_FAIL(GT_IS_RESOURCE_DOWNLOADER(self));
    RETURN_IF_FAIL(!utils_str_empty(image_filetype));

    GtResourceDownloaderPrivate* priv = gt_resource_downloader_get_instance_private(self);

    priv->image_filetype = g_strdup(image_filetype);
}

/* FIXME: Make cancellable */
GdkPixbuf*
gt_resource_downloader_download_image_immediately(GtResourceDownloader* self,
    const gchar* uri, const gchar* name, ResourceDownloaderFunc cb,
    gpointer udata, GError** error)
{
    RETURN_VAL_IF_FAIL(GT_IS_RESOURCE_DOWNLOADER(self), NULL);
    RETURN_VAL_IF_FAIL(!utils_str_empty(uri), NULL);

    GtResourceDownloaderPrivate* priv = gt_resource_downloader_get_instance_private(self);
    g_autofree gchar* filename = NULL;
    g_autoptr(GdkPixbuf) ret = NULL;
    g_autoptr(GError) err = NULL;
    g_autoptr(SoupMessage) msg = NULL;
    ResourceData* data = NULL;

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

    if (priv->filepath && g_file_test(filename, G_FILE_TEST_EXISTS))
    {
        ret = gdk_pixbuf_new_from_file(filename, &err);

        if (err)
        {
            WARNING("Unable to download image because: %s", err->message);

            g_propagate_prefixed_error(error, g_steal_pointer(&err),
                "Unable to download image because: ");

            /* NOTE: Don't return here as we still might be able to
             * download a new image*/
        }
    }

    msg = soup_message_new(SOUP_METHOD_GET, uri);
    soup_message_headers_append(msg->request_headers, "Client-ID", CLIENT_ID);

    data = resource_data_new();
    data->uri = g_strdup(uri);
    data->name = g_strdup(name);
    data->cb = cb;
    data->udata = udata;
    data->self = g_object_ref(self);
    data->msg = g_steal_pointer(&msg);

    soup_session_send_async(priv->soup, data->msg, NULL, send_message_cb, data);

    /* NOTE: Return any found image immediately */
    return g_steal_pointer(&ret);
}
