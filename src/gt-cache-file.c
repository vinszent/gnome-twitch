/*
 *  This file is part of GNOME Twitch - 'Enjoy Twitch on your GNU/Linux desktop'
 *  Copyright © 2017 Vincent Szolnoky <vinszent@vinszent.com>
 *
 *  GNOME Twitch is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GNOME Twitch is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNOME Twitch. If not, see <http://www.gnu.org/licenses/>.
 */

#include "gt-cache-file.h"
#include "gt-cache.h"
#include "utils.h"
#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#define TAG "GtCacheFile"
#include "gnome-twitch/gt-log.h"

#define DB_FILENAME "cache.json"

#define KEY_MEMBER_NAME "key"
#define ID_MEMBER_NAME "id"
#define CREATED_MEMBER_NAME "created"
#define EXPIRY_MEMBER_NAME "expiry"
#define ETAG_MEMBER_NAME "etag"

typedef struct
{
    GCancellable* cancel;
    GHashTable* db;

    gchar* cache_directory;
} GtCacheFilePrivate;

typedef struct
{
    gchar* id;
    gchar* key;
    GDateTime* created;
    GDateTime* expiry;
    gchar* etag;
} GtCacheFileEntry;

static void gt_cache_iface_init(GtCacheInterface* iface);

G_DEFINE_TYPE_WITH_CODE(GtCacheFile, gt_cache_file, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(GT_TYPE_CACHE, gt_cache_iface_init)
    G_ADD_PRIVATE(GtCacheFile));

enum
{
    PROP_0,
    PROP_CACHE_DIRECTORY,
    NUM_PROPS,
};

static GParamSpec* props[NUM_PROPS];

static GtCacheFileEntry*
gt_cache_file_entry_new()
{
    GtCacheFileEntry* entry = g_slice_new0(GtCacheFileEntry);

    entry->id = g_uuid_string_random();
    entry->created = g_date_time_new_now_utc();

    return entry;
}

static GtCacheFileEntry*
gt_cache_file_entry_new_with_params(const gchar* key, GDateTime* expiry, const gchar* etag)
{
    GtCacheFileEntry* entry = gt_cache_file_entry_new();

    entry->key = g_strdup(key);
    entry->expiry = g_date_time_ref(expiry);
    entry->etag = g_strdup(etag);

    return entry;
}

static void
gt_cache_file_entry_update(GtCacheFileEntry* entry, GDateTime* expiry, const gchar* etag)
{
    RETURN_IF_FAIL(entry != NULL);

    g_date_time_unref(entry->created);
    g_date_time_unref(entry->expiry);
    g_free(entry->etag);

    entry->created = g_date_time_new_now_utc();
    entry->expiry = g_date_time_ref(expiry);
    entry->etag = g_strdup(etag);
}

static void
gt_cache_file_entry_free(GtCacheFileEntry* entry)
{
    if (!entry) return;

    g_free(entry->key);
    if (entry->created) g_date_time_unref(entry->created);
    if (entry->expiry) g_date_time_unref(entry->expiry);
    g_free(entry->etag);
    g_slice_free(GtCacheFileEntry, entry);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtCacheFileEntry, gt_cache_file_entry_free)

static void
save_db(GtCacheFile* self)
{
    GtCacheFilePrivate* priv = gt_cache_file_get_instance_private(self);

    g_autofree gchar* filename = g_build_filename(priv->cache_directory, "cache.json", NULL);
    g_autoptr(GError) err = NULL;
    GHashTableIter iter;
    gpointer key = NULL;
    gpointer value = NULL;
    g_autoptr(JsonBuilder) builder = json_builder_new();
    g_autoptr(JsonGenerator) generator = NULL;

    g_hash_table_iter_init(&iter, priv->db);
    json_builder_begin_object(builder);
    while(g_hash_table_iter_next(&iter, &key, &value))
    {
        RETURN_IF_FAIL(value != NULL);

        const gchar* id = key;
        const GtCacheFileEntry* entry = value;

        json_builder_set_member_name(builder, id);
        json_builder_begin_object(builder);

        json_builder_set_member_name(builder, ID_MEMBER_NAME);
        json_builder_add_string_value(builder, entry->id);

        json_builder_set_member_name(builder, CREATED_MEMBER_NAME);
        json_builder_add_int_value(builder, g_date_time_to_unix(entry->created));

        json_builder_set_member_name(builder, EXPIRY_MEMBER_NAME);
        json_builder_add_int_value(builder, g_date_time_to_unix(entry->expiry));

        json_builder_set_member_name(builder, ETAG_MEMBER_NAME);
        json_builder_add_string_value(builder, entry->etag);

        json_builder_end_object(builder);
    }
    json_builder_end_object(builder);

    generator = json_generator_new();
    json_generator_set_root(generator, json_builder_get_root(builder));

    json_generator_to_file(generator, filename, &err);

    MESSAGE("Saved '%d' cache entries to file", g_hash_table_size(priv->db));

    /* TODO: Put object into an error state */
    RETURN_IF_ERROR(err);
}

static void
load_db(GtCacheFile* self)
{
    GtCacheFilePrivate* priv = gt_cache_file_get_instance_private(self);

    g_autofree gchar* filename = g_build_filename(priv->cache_directory, "cache.json", NULL);

    if (!g_file_test(filename, G_FILE_TEST_EXISTS))
    {
        INFO("No cache db file at '%s'", filename);
        return;
    }

    g_autoptr(JsonParser) parser = json_parser_new();
    g_autoptr(JsonReader) reader = NULL;
    g_autoptr(GError) err = NULL;

    json_parser_load_from_file(parser, filename, &err);

    /* TODO: Put object into an error state */
    RETURN_IF_ERROR(err);

    reader = json_reader_new(json_parser_get_root(parser));

    /* TODO: Read version number */

    for (gint i = 0; i < json_reader_count_members(reader); i++)
    {
        g_autoptr(GtCacheFileEntry) entry = gt_cache_file_entry_new();

        /* TODO: Handle error */
        if (!json_reader_read_element(reader, i))
            RETURN_IF_ERROR(json_reader_get_error(reader));

        entry->key = g_strdup(json_reader_get_member_name(reader));

        RETURN_IF_FAIL(entry->key != NULL);

        /* TODO: Handle error */
        if (!json_reader_read_member(reader, ID_MEMBER_NAME))
            RETURN_IF_ERROR(json_reader_get_error(reader));
        RETURN_IF_FAIL(!json_reader_get_null_value(reader));
        entry->id = g_strdup(json_reader_get_string_value(reader));
        json_reader_end_member(reader);

        /* TODO: Put object into an error state */
        if (!json_reader_read_member(reader, CREATED_MEMBER_NAME))
            RETURN_IF_ERROR(json_reader_get_error(reader));
        entry->created = g_date_time_new_from_unix_utc(json_reader_get_int_value(reader));
        json_reader_end_member(reader);

        /* TODO: Put object into an error state */
        if (!json_reader_read_member(reader, EXPIRY_MEMBER_NAME))
            RETURN_IF_ERROR(json_reader_get_error(reader));
        entry->expiry = g_date_time_new_from_unix_utc(json_reader_get_int_value(reader));
        json_reader_end_member(reader);

        /* TODO: Handle error */
        if (!json_reader_read_member(reader, ETAG_MEMBER_NAME))
            RETURN_IF_ERROR(json_reader_get_error(reader));
        entry->etag = json_reader_get_null_value(reader) ?
            NULL : g_strdup(json_reader_get_string_value(reader));
        json_reader_end_member(reader);

        json_reader_end_element(reader);

        g_hash_table_insert(priv->db, g_strdup(entry->key), entry);
        g_steal_pointer(&entry);
    }

    MESSAGE("Loaded '%d' cache entries from file", g_hash_table_size(priv->db));
}

static void
write_data_cb(GObject* source,
    GAsyncResult* res, gpointer udata)
{
    RETURN_IF_FAIL(G_IS_FILE(source));
    RETURN_IF_FAIL(G_IS_ASYNC_RESULT(res));
    RETURN_IF_FAIL(udata != NULL);

    g_autoptr(GError) err = NULL;

    g_file_replace_contents_finish(G_FILE(source), res, NULL, &err);

    g_free(udata);

    if (err)
        WARNING("Couldn't write cache file data because: %s, failing silently", err->message);
}

static void
save_data(GtCache* cache, const gchar* key, gconstpointer data, gsize length,
    GDateTime* last_updated, GDateTime* expiry, const gchar* etag)
{
    RETURN_IF_FAIL(GT_IS_CACHE_FILE(cache));
    RETURN_IF_FAIL(!utils_str_empty(key));
    RETURN_IF_FAIL(data != NULL && length != 0);
    RETURN_IF_FAIL(!(last_updated == NULL && etag == NULL));

    GtCacheFilePrivate* priv = gt_cache_file_get_instance_private(GT_CACHE_FILE(cache));

    GtCacheFileEntry* entry = NULL; /* NOTE: Don't free, owned by hash table */
    g_autofree gchar* filename = NULL;
    g_autoptr(GFile) file = NULL;
    gpointer copied_data;

    if ((entry = g_hash_table_lookup(priv->db, key)) != NULL)
    {
        gt_cache_file_entry_update(entry, expiry, etag);
    }
    else
    {
        entry = gt_cache_file_entry_new_with_params(key, expiry, etag);

        g_hash_table_insert(priv->db, g_strdup(key), entry);
    }

    filename = g_build_filename(priv->cache_directory, entry->id ,NULL);

    file = g_file_new_for_path(filename);

    /* NOTE: Will be free'd in callback */
    copied_data = g_memdup(data, length);

    g_file_replace_contents_async(file, copied_data, length, NULL, FALSE,
        G_FILE_CREATE_REPLACE_DESTINATION, NULL, write_data_cb, copied_data);
}

gboolean
is_data_stale(GtCache* cache, const gchar* key, GDateTime* last_updated, const gchar* etag)
{
    RETURN_VAL_IF_FAIL(GT_IS_CACHE_FILE(cache), TRUE);
    RETURN_VAL_IF_FAIL(!utils_str_empty(key), TRUE);
    RETURN_VAL_IF_FAIL(!(last_updated == NULL && etag == NULL), TRUE);

    GtCacheFilePrivate* priv = gt_cache_file_get_instance_private(GT_CACHE_FILE(cache));

    GtCacheFileEntry* entry = NULL; /* NOTE: Don't free, owned by hash table */

    if ((entry = g_hash_table_lookup(priv->db, key)) != NULL)
    {
        g_autoptr(GDateTime) now = g_date_time_new_now_utc();

        /* NOTE: Past expiry date */
        if (g_date_time_compare(now, entry->expiry) == 1)
        {
            DEBUG("Cache miss: Past expiry date");
            return TRUE;
        }

        /* NOTE: Check if newly updated */
        if (last_updated && g_date_time_compare(last_updated, entry->created) == 1)
        {
            DEBUG("Cache miss: Newly updated");
            return TRUE;
        }

        /* NOTE: Check if etag differs */
        if (etag && entry->etag && g_strcmp0(etag, entry->etag) != 0)
        {
            DEBUG("Cache miss: Etags differ");
            return TRUE;
        }
    }
    else
        return TRUE; /* NOTE: Return true for data that doesn't exist */

    return FALSE;
}

GInputStream*
get_data_stream(GtCache* cache, const gchar* key, GError** error)
{
    RETURN_VAL_IF_FAIL(GT_IS_CACHE_FILE(cache), NULL);
    RETURN_VAL_IF_FAIL(!utils_str_empty(key), NULL);

    GtCacheFile* self = GT_CACHE_FILE(cache);
    GtCacheFilePrivate* priv = gt_cache_file_get_instance_private(self);

    g_autofree gchar* filename = NULL;
    g_autoptr(GFile) file = NULL;
    const GtCacheFileEntry* entry = NULL;
    g_autoptr(GInputStream) istream = NULL;
    g_autoptr(GError) err = NULL;

    entry = g_hash_table_lookup(priv->db, key);

    if (entry == NULL)
    {
        g_set_error(error, GT_CACHE_ERROR, GT_CACHE_ERROR_ENTRY_NOT_FOUND,
            "Couldn't find entry for given key '%s'", key);

        return NULL;
    }

    filename = g_build_filename(priv->cache_directory, entry->id, NULL);

    file = g_file_new_for_path(filename);

    istream = G_INPUT_STREAM(g_file_read(file, NULL, &err));

    if (err)
    {
        g_propagate_prefixed_error(error, g_steal_pointer(&err),
            "Unable to get data stream for '%s' because: ", filename);

        return NULL;
    }

    return g_steal_pointer(&istream);
}

static void
dispose(GObject* obj)
{
    RETURN_IF_FAIL(GT_IS_CACHE_FILE(obj));

    GtCacheFile* self = GT_CACHE_FILE(obj);
    GtCacheFilePrivate* priv = gt_cache_file_get_instance_private(self);

    save_db(self);

    g_hash_table_unref(priv->db);

    G_OBJECT_CLASS(gt_cache_file_parent_class)->dispose(obj);
}

static void
finalize(GObject* obj)
{
    RETURN_IF_FAIL(GT_IS_CACHE_FILE(obj));

    GtCacheFile* self = GT_CACHE_FILE(obj);
    GtCacheFilePrivate* priv = gt_cache_file_get_instance_private(self);

    g_free(priv->cache_directory);

    G_OBJECT_CLASS(gt_cache_file_parent_class)->finalize(obj);
}

static void
get_property(GObject* obj,
    guint prop, GValue* val, GParamSpec* pspec)
{
    RETURN_IF_FAIL(GT_IS_CACHE_FILE(obj));
    RETURN_IF_FAIL(G_IS_VALUE(val));
    RETURN_IF_FAIL(G_IS_PARAM_SPEC(pspec));

    GtCacheFile* self = GT_CACHE_FILE(obj);
    GtCacheFilePrivate* priv = gt_cache_file_get_instance_private(self);

    switch (prop)
    {
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
    RETURN_IF_FAIL(GT_IS_CACHE_FILE(obj));
    RETURN_IF_FAIL(G_IS_VALUE(val));
    RETURN_IF_FAIL(G_IS_PARAM_SPEC(pspec));

    GtCacheFile* self = GT_CACHE_FILE(obj);
    GtCacheFilePrivate* priv = gt_cache_file_get_instance_private(self);

    switch (prop)
    {
        case PROP_CACHE_DIRECTORY:
            g_free(priv->cache_directory);
            priv->cache_directory = g_value_dup_string(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
constructed(GObject* obj)
{
    RETURN_IF_FAIL(GT_IS_CACHE_FILE(obj));

    GtCacheFile* self = GT_CACHE_FILE(obj);

    load_db(self);

    G_OBJECT_CLASS(gt_cache_file_parent_class)->constructed(obj);
}

static void
gt_cache_iface_init(GtCacheInterface* iface)
{
    iface->save_data = save_data;
    iface->get_data_stream = get_data_stream;
    iface->is_data_stale = is_data_stale;
}

static void
gt_cache_file_class_init(GtCacheFileClass* klass)
{
    GObjectClass* obj_class = G_OBJECT_CLASS(klass);

    obj_class->set_property = set_property;
    obj_class->get_property = get_property;
    obj_class->finalize = finalize;
    obj_class->dispose = dispose;
    obj_class->constructed = constructed;

    g_autofree gchar* default_cache_directory = g_build_filename(g_get_user_cache_dir(),
        "gnome-twitch", "http-cache", NULL);

    props[PROP_CACHE_DIRECTORY] = g_param_spec_string("cache-directory",
        "Cache directory", "Directory where cached files should be placed",
        default_cache_directory, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    g_object_class_override_property(obj_class, PROP_CACHE_DIRECTORY, "cache-directory");
}

static void
gt_cache_file_init(GtCacheFile* self)
{
    g_assert(GT_IS_CACHE_FILE(self));

    GtCacheFilePrivate* priv = gt_cache_file_get_instance_private(self);

    priv->db = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) gt_cache_file_entry_free);
}

GtCacheFile*
gt_cache_file_new()
{
    return g_object_new(GT_TYPE_CACHE_FILE, NULL);
}
