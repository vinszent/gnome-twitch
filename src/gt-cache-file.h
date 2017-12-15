#ifndef GT_CACHE_FILE_H
#define GT_CACHE_FILE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GT_TYPE_CACHE_FILE gt_cache_file_get_type()

G_DECLARE_FINAL_TYPE(GtCacheFile, gt_cache_file, GT, CACHE_FILE, GObject);

struct _GtCacheFile
{
    GObject parent_instance;
};

GtCacheFile* gt_cache_file_new();

G_END_DECLS

#endif
