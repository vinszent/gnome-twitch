#ifndef GT_CACHE_H
#define GT_CACHE_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GT_TYPE_CACHE gt_cache_get_type()

G_DECLARE_INTERFACE(GtCache, gt_cache, GT, CACHE, GObject);

#define GT_CACHE_ERROR g_quark_from_static_string("gt-cache-error-quark")

typedef enum
{
    GT_CACHE_ERROR_ENTRY_NOT_FOUND,
} GtCacheError;

struct _GtCacheInterface
{
    GTypeInterface parent_interface;

    void (*save_data) (GtCache* self, const gchar* key, gconstpointer data, gsize length, GDateTime* last_updated, GDateTime* expiry, const gchar* etag);
    GInputStream* (*get_data_stream) (GtCache* self, const gchar* key, GError** error);
    gboolean (*is_data_stale) (GtCache* self, const gchar* key, GDateTime* last_updated, const gchar* etag);
};

/* TODO: Add docs */
void gt_cache_save_data(GtCache* self, const gchar* key, gconstpointer data, gsize length, GDateTime* last_updated, GDateTime* expiry, const gchar* etag);
GInputStream* gt_cache_get_data_stream(GtCache* self, const gchar* key, GError** error);
gboolean gt_cache_is_data_stale(GtCache* self, const gchar* key, GDateTime* last_updated, const gchar* etag);

G_END_DECLS

#endif
