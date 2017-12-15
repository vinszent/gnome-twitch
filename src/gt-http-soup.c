#include "gt-http-soup.h"
#include "gt-http.h"
#include "gt-cache.h"
#include "gt-cache-file.h"
#include "utils.h"
#include "config.h"
#include <libsoup/soup.h>

#define TAG "GtHTTPSoup"
#include "gnome-twitch/gt-log.h"

#define BUFFER_SIZE 2*1000*1024 /* NOTE: 2 MB */

typedef struct
{
    SoupSession* soup;
    GQueue* message_queue;
    GHashTable* inflight_table;
    GtCache* cache;

    guint max_inflight_per_category;
    gchar* cache_directory;
} GtHTTPSoupPrivate;

typedef struct
{
    GWeakRef* self;
    SoupMessage* soup_message;
    gchar* uri;
    gchar* category;
    GCancellable* cancel;
    gulong cancel_cb_id;
    GtHTTPCallback cb;
    gpointer udata;
    gint flags;
    gssize content_length;
    gssize bytes_read;
} SoupCallbackData;

static void gt_http_iface_init(GtHTTPInterface* iface);

G_DEFINE_TYPE_WITH_CODE(GtHTTPSoup, gt_http_soup, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(GT_TYPE_HTTP, gt_http_iface_init)
    G_ADD_PRIVATE(GtHTTPSoup));

enum
{
    PROP_0,
    PROP_MAX_INFLIGHT_PER_CATEGORY,
    PROP_CACHE_DIRECTORY,
    NUM_PROPS,
};

static GParamSpec* props[NUM_PROPS];

#define NO_CATEGORY "_NO_CATEGORY"

static SoupCallbackData*
soup_callback_data_new(GtHTTPSoup* self, SoupMessage* soup_message, const gchar* category,
    GCancellable* cancel, GtHTTPCallback cb, gpointer udata, gint flags)
{
    SoupCallbackData* data = g_slice_new0(SoupCallbackData);

    data->self = utils_weak_ref_new(self);
    data->soup_message = g_object_ref(soup_message);
    data->uri = soup_uri_to_string(soup_message_get_uri(soup_message), FALSE);
    data->category = g_strdup(category);
    data->cancel = g_object_ref(cancel);
    data->cb = cb;
    data->udata = udata;
    data->flags = flags;

    return data;
}

static void
soup_callback_data_free(SoupCallbackData* data)
{
    if (!data) return;

    g_free(data->uri);
    g_free(data->category);
    utils_weak_ref_free(data->self);
    g_object_unref(data->soup_message);
    if (data->cancel) g_object_unref(data->cancel);

    g_slice_free(SoupCallbackData, data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(SoupCallbackData, soup_callback_data_free);

static inline void send_next_message(GtHTTPSoup* self);

static inline void
decrement_inflight_for_category(GtHTTPSoup* self, const gchar* category)
{
    GtHTTPSoupPrivate* priv = gt_http_soup_get_instance_private(self);
    gpointer lookup = NULL;
    guint inflight;

    lookup = g_hash_table_lookup(priv->inflight_table, category);

    if (!lookup)
    {
        WARNING("Unable to decrement inflight for category '%s', silently ignoring", category);
        return;
    }

    inflight = GPOINTER_TO_UINT(lookup);

    g_hash_table_insert(priv->inflight_table, g_strdup(category), GUINT_TO_POINTER(inflight - 1));

    DEBUG("Inflight for category '%s' '%u'", category, inflight);
}

static inline void
increment_inflight_for_category(GtHTTPSoup* self, const gchar* category)
{
    GtHTTPSoupPrivate* priv = gt_http_soup_get_instance_private(self);
    gpointer lookup = NULL;
    guint inflight;

    lookup = g_hash_table_lookup(priv->inflight_table, category);

    if (!lookup)
        TRACE("Couldn't find category '%s' in table, inserting new entry", category);

    inflight = GPOINTER_TO_UINT(lookup);

    g_hash_table_insert(priv->inflight_table, g_strdup(category), GUINT_TO_POINTER(inflight + 1));

    DEBUG("Inflight for category '%s' '%u'", category, inflight);
}

static gboolean
can_cache_response(SoupMessage* msg)
{
    if (soup_message_headers_header_contains(msg->response_headers, "Pragma", "no-cache"))
        return FALSE;
    if (soup_message_headers_header_contains(msg->response_headers, "Cache-Control", "no-cache"))
        return FALSE;
    if (soup_message_headers_header_contains(msg->response_headers, "Expires", "0"))
        return FALSE;

    return TRUE;
}

static GDateTime*
parse_http_time(const gchar* time)
{
    GDateTime* ret = NULL;
    g_autoptr(SoupDate) soup_date = soup_date_new_from_string(time);

    ret = g_date_time_new_from_unix_utc(soup_date_to_time_t(soup_date));

    return ret;
}

static void
download_stream_fill_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    RETURN_IF_FAIL(G_IS_BUFFERED_INPUT_STREAM(source));
    RETURN_IF_FAIL(G_IS_ASYNC_RESULT(res));
    RETURN_IF_FAIL(udata != NULL);

    g_autoptr(SoupCallbackData) msg = udata;
    g_autoptr(GError) err = NULL;

    g_autoptr(GtHTTPSoup) self = g_weak_ref_get(msg->self);

    if (!self) { TRACE("Unreffed while waiting"); return; }

    GtHTTPSoupPrivate* priv = gt_http_soup_get_instance_private(self);
    GBufferedInputStream* bistream = G_BUFFERED_INPUT_STREAM(source);

    msg->bytes_read += g_buffered_input_stream_fill_finish(bistream, res, &err);

    if (err)
    {
        if (!g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
            g_prefix_error(&err, "Unable to cache request to '%s' because: ", msg->uri);

            WARNING("%s", err->message);
        }

        msg->cb(GT_HTTP(self), NULL, g_steal_pointer(&err), msg->udata);

        return;
    }

    if (msg->bytes_read < msg->content_length)
    {
        g_buffered_input_stream_fill_async(bistream, msg->content_length,
            G_PRIORITY_DEFAULT, msg->cancel, download_stream_fill_cb, msg);
        g_steal_pointer(&msg);
    }
    else
    {
        const gchar* last_modified = soup_message_headers_get_one(msg->soup_message->response_headers, "Last-Modified");
        const gchar* etag = soup_message_headers_get_one(msg->soup_message->response_headers, "ETag");
        const gchar* expires = soup_message_headers_get_one(msg->soup_message->response_headers, "Expires");

        g_autoptr(GError) err = NULL;
        g_autoptr(GDateTime) last_updated = parse_http_time(last_modified);

        if (err)
        {
            g_prefix_error(&err, "Unable to cache request to '%s' because: ", msg->uri);

            WARNING("%s", err->message);

            msg->cb(GT_HTTP(self), NULL, g_steal_pointer(&err), msg->udata);
        }
        else
        {
            gsize length;
            gconstpointer data = g_buffered_input_stream_peek_buffer(bistream, &length);
            g_autoptr(GDateTime) expiry = parse_http_time(expires);

            /* TODO: Handle error */
            RETURN_IF_ERROR(err);

            gt_cache_save_data(priv->cache, msg->uri, data, length, last_updated, expiry, etag);
            msg->cb(GT_HTTP(self), bistream, NULL, msg->udata);
        }
    }
}

static void
download_response(GtHTTPSoup* self, GInputStream* istream, SoupCallbackData* msg)
{
    GtHTTPSoupPrivate* priv = gt_http_soup_get_instance_private(self);

    const gchar* last_modified = soup_message_headers_get_one(msg->soup_message->response_headers, "Last-Modified");
    const gchar* etag = soup_message_headers_get_one(msg->soup_message->response_headers, "etag");
    const gchar* expires = soup_message_headers_get_one(msg->soup_message->response_headers, "expires");

    g_autoptr(GDateTime) last_updated = parse_http_time(last_modified);

    if (gt_cache_is_data_stale(priv->cache, msg->uri, last_updated, etag))
    {
        g_autoptr(GBufferedInputStream) bistream = G_BUFFERED_INPUT_STREAM(g_buffered_input_stream_new_sized(istream, BUFFER_SIZE));

        DEBUG("Cache miss for '%s'", msg->uri);

        if (msg->content_length > BUFFER_SIZE)
        {
            g_autoptr(GError) err = NULL;

            g_set_error(&err, GT_HTTP_ERROR, GT_HTTP_ERROR_UNKNOWN,
                "Content length '%ld' greater than buffer size, not downloading response", msg->content_length);
            WARNING("%s", err->message);

            msg->cb(GT_HTTP(self), NULL, g_steal_pointer(&err), msg->udata);

            return;
        }

        g_buffered_input_stream_fill_async(bistream, msg->content_length,
            G_PRIORITY_DEFAULT, msg->cancel, download_stream_fill_cb, msg);
    }
    else
    {
        g_autoptr(GError) err = NULL;
        g_autoptr(GInputStream) fistream = gt_cache_get_data_stream(priv->cache, msg->uri, &err);

        DEBUG("Cache hit for '%s'", msg->uri);

        if (err)
        {
            g_prefix_error(&err, "Couldn't get data stream for cached file because: ");
            WARNING("%s", err->message);
            msg->cb(GT_HTTP(self), NULL, g_steal_pointer(&err), msg->udata);
        }
        else
        {
            msg->cb(GT_HTTP(self), g_steal_pointer(&fistream), NULL, msg->udata);
            soup_callback_data_free(msg);
        }
    }
}


/* NOTE: This function is to remove a message from the queue if it has
 * been cancelled but not sent yet */
static void
msg_cancelled_cb(GCancellable* cancel, gpointer udata)
{
    RETURN_IF_FAIL(udata != NULL);

    SoupCallbackData* data = udata;
    g_autoptr(GtHTTPSoup) self = g_weak_ref_get(data->self);

    if (!self) {TRACE("Unreffed"); return;}

    GtHTTPSoupPrivate* priv = gt_http_soup_get_instance_private(self);

    g_queue_remove(priv->message_queue, data);
}

static void
soup_message_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    RETURN_IF_FAIL(SOUP_IS_SESSION(source));
    RETURN_IF_FAIL(G_IS_ASYNC_RESULT(res));
    RETURN_IF_FAIL(udata != NULL);

    g_autoptr(SoupCallbackData) data = udata;
    g_autoptr(GtHTTPSoup) self = g_weak_ref_get(data->self);

    if (!self) { TRACE("Unreffed while waiting"); return; }

    GtHTTPSoupPrivate* priv = gt_http_soup_get_instance_private(self);
    g_autoptr(GInputStream) istream = NULL;
    g_autoptr(GError) err = NULL;

    decrement_inflight_for_category(self, data->category);

    istream = soup_session_send_finish(priv->soup, res, &err);

    /* NOTE: Manually handle cancelled request here */
    if (g_cancellable_is_cancelled(data->cancel))
    {
        g_set_error(&err, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled");

        data->cb(GT_HTTP(self), NULL, g_steal_pointer(&err), data->udata);

        goto send_next_message;
    }

    if (err)
    {
        if (!g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
            g_prefix_error(&err, "Unable to send message to '%s' with category '%s' because: %s",
                data->uri, data->category, err->message);

            WARNING("%s", err->message);
        }

        data->cb(GT_HTTP(self), NULL, g_steal_pointer(&err), data->udata);

        goto send_next_message;
    }

    if (!SOUP_STATUS_IS_SUCCESSFUL(data->soup_message->status_code))
    {
        g_set_error(&err, GT_HTTP_ERROR, GT_HTTP_ERROR_UNSUCCESSFUL_RESPONSE, "Received unsuccesful response '%d:%s' from uri '%s'",
            data->soup_message->status_code, soup_status_get_phrase(data->soup_message->status_code), data->uri);

        WARNING("%s", err->message);

        data->cb(GT_HTTP(self), NULL, g_steal_pointer(&err), data->udata);

        goto send_next_message;
    }

    if (data->flags & GT_HTTP_FLAG_CACHE_RESPONSE && can_cache_response(data->soup_message))
    {
        data->content_length = soup_message_headers_get_content_length(data->soup_message->response_headers);
        download_response(self, istream, g_steal_pointer(&data));
    }
    else if (data->flags & GT_HTTP_FLAG_RETURN_STREAM)
        data->cb(GT_HTTP(self), istream, NULL, data->udata);

send_next_message:
    send_next_message(self);
}

static inline void
send_next_message(GtHTTPSoup* self)
{
    GtHTTPSoupPrivate* priv = gt_http_soup_get_instance_private(self);
    guint inflight = 0;
    guint i = 0;
    SoupCallbackData* next_msg = NULL; /* NOTE: Doesn't need free */

    for (i = 0; i < g_queue_get_length(priv->message_queue); i++)
    {
        next_msg = g_queue_peek_nth(priv->message_queue, i);

        inflight = GPOINTER_TO_UINT(g_hash_table_lookup(priv->inflight_table, next_msg->category));

        if (inflight < priv->max_inflight_per_category)
            break;
    }

    next_msg = g_queue_pop_nth(priv->message_queue, i);

    if (next_msg)
    {
        g_cancellable_disconnect(next_msg->cancel, next_msg->cancel_cb_id);

        increment_inflight_for_category(self, next_msg->category);

        /* NOTE: Cancelling a async request will cause SoupSession to
         * segfault so we don't allow cancelling here. Instead we will
         * handle it manually
         *
         * See: https://bugzilla.gnome.org/show_bug.cgi?id=771912 */

        soup_session_send_async(priv->soup, next_msg->soup_message,
            /* next_msg->cancel */NULL, soup_message_cb, next_msg); /* NOTE: Assumes ownership of next_msg */
    }
}

static void
get_with_category(GtHTTP* http, const gchar* uri, const gchar* category, gchar** headers,
    GCancellable* cancel, GtHTTPCallback cb, gpointer udata, gint flags)
{
    RETURN_IF_FAIL(GT_HTTP_SOUP(http));
    RETURN_IF_FAIL(!utils_str_empty(uri));
    RETURN_IF_FAIL(!utils_str_empty(category));
    RETURN_IF_FAIL(flags != 0);

    GtHTTPSoup* self = GT_HTTP_SOUP(http);
    GtHTTPSoupPrivate* priv = gt_http_soup_get_instance_private(self);

    g_autoptr(SoupMessage) soup_msg = NULL;
    g_autoptr(SoupCallbackData) data = NULL;

    soup_msg = soup_message_new(SOUP_METHOD_GET, uri);

    for (guint i = 0; ; i += 2)
    {
        const gchar* key = headers[i];

        if (!key) break;

        const gchar* val = headers[i+1];

        soup_message_headers_append(soup_msg->request_headers, key, val);
    }

    data = soup_callback_data_new(self, soup_msg,
        category, cancel, cb, udata, flags);

    data->cancel_cb_id = g_cancellable_connect(cancel, G_CALLBACK(msg_cancelled_cb), data, NULL);

    g_queue_push_tail(priv->message_queue, g_steal_pointer(&data));

    send_next_message(self);
}

static void
get(GtHTTP* http, const gchar* uri, gchar** headers,
    GCancellable* cancel, GtHTTPCallback cb, gpointer udata, gint flags)
{
    get_with_category(http, uri, NO_CATEGORY, headers, cancel, cb, udata, flags);
}

static void
dispose(GObject* obj)
{
    RETURN_IF_FAIL(GT_IS_HTTP_SOUP(obj));

    GtHTTPSoup* self = GT_HTTP_SOUP(obj);
    GtHTTPSoupPrivate* priv = gt_http_soup_get_instance_private(self);

    g_object_unref(priv->soup);
    g_hash_table_unref(priv->inflight_table);
    g_object_unref(priv->cache);

    G_OBJECT_CLASS(gt_http_soup_parent_class)->dispose(obj);
}

static void
finalize(GObject* obj)
{
    RETURN_IF_FAIL(GT_IS_HTTP_SOUP(obj));

    GtHTTPSoup* self = GT_HTTP_SOUP(obj);
    GtHTTPSoupPrivate* priv = gt_http_soup_get_instance_private(self);

    g_queue_free_full(priv->message_queue, (GDestroyNotify) soup_callback_data_free);
    g_free(priv->cache_directory);

    G_OBJECT_CLASS(gt_http_soup_parent_class)->finalize(obj);
}

static void
get_property(GObject* obj,
    guint prop, GValue* val, GParamSpec* pspec)
{
    RETURN_IF_FAIL(GT_IS_HTTP_SOUP(obj));
    RETURN_IF_FAIL(G_IS_VALUE(val));
    RETURN_IF_FAIL(G_IS_PARAM_SPEC(pspec));

    GtHTTPSoup* self = GT_HTTP_SOUP(obj);
    GtHTTPSoupPrivate* priv = gt_http_soup_get_instance_private(self);

    switch (prop)
    {
        case PROP_MAX_INFLIGHT_PER_CATEGORY:
            g_value_set_uint(val, priv->max_inflight_per_category);
            break;
        case PROP_CACHE_DIRECTORY:
            g_value_set_string(val, priv->cache_directory);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
set_property(GObject* obj,
    guint prop, const GValue* val, GParamSpec* pspec)
{
    RETURN_IF_FAIL(GT_IS_HTTP_SOUP(obj));
    RETURN_IF_FAIL(G_IS_VALUE(val));
    RETURN_IF_FAIL(G_IS_PARAM_SPEC(pspec));

    GtHTTPSoup* self = GT_HTTP_SOUP(obj);
    GtHTTPSoupPrivate* priv = gt_http_soup_get_instance_private(self);

    switch (prop)
    {
        case PROP_MAX_INFLIGHT_PER_CATEGORY:
            priv->max_inflight_per_category = g_value_get_uint(val);
            break;
        case PROP_CACHE_DIRECTORY:
            g_free(priv->cache_directory);
            priv->cache_directory = g_value_dup_string(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_http_iface_init(GtHTTPInterface* iface)
{
    iface->get = get;
    iface->get_with_category = get_with_category;
}

static void
gt_http_soup_class_init(GtHTTPSoupClass* klass)
{
    GObjectClass* obj_class = G_OBJECT_CLASS(klass);

    obj_class->get_property = get_property;
    obj_class->set_property = set_property;
    obj_class->finalize = finalize;
    obj_class->dispose = dispose;

    g_autofree gchar* default_cache_directory = g_build_filename(g_get_user_cache_dir(),
        "gnome-twitch", "http-cache", NULL);

    props[PROP_MAX_INFLIGHT_PER_CATEGORY] = g_param_spec_uint("max-inflight-per-category",
        "Max inflight per category", "Maximum allowed inflight messages per category",
        0, G_MAXUINT, 4, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    props[PROP_CACHE_DIRECTORY] = g_param_spec_string("cache-directory",
        "Cache directory", "Directory where cached files should be placed",
        default_cache_directory, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    g_object_class_override_property(obj_class, PROP_MAX_INFLIGHT_PER_CATEGORY, "max-inflight-per-category");
    g_object_class_override_property(obj_class, PROP_CACHE_DIRECTORY, "cache-directory");
}

static void
gt_http_soup_init(GtHTTPSoup* self)
{
    GtHTTPSoupPrivate* priv = gt_http_soup_get_instance_private(self);

    priv->soup = soup_session_new();
    priv->message_queue = g_queue_new();
    priv->inflight_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    priv->cache = GT_CACHE(gt_cache_file_new()); /* TODO: Use libpeas to load this dynamically */
}

GtHTTPSoup*
gt_http_soup_new()
{
    return g_object_new(GT_TYPE_HTTP_SOUP, NULL);
}
