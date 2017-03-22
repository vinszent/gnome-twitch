#ifndef GT_RESOURCE_DOWNLOADER_H
#define GT_RESOURCE_DOWNLOADER_H

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define GT_TYPE_RESOURCE_DOWNLOADER gt_resource_downloader_get_type()

#define GT_IMAGE_FILETYPE_JPEG "jpeg"
#define GT_IMAGE_FILETYPE_PNG "png"

typedef void (*ResourceDownloaderFunc)(GdkPixbuf* pixbuf, gpointer udata, GError* err);

G_DECLARE_FINAL_TYPE(GtResourceDownloader, gt_resource_downloader, GT, RESOURCE_DOWNLOADER, GObject);

struct _GtResourceDownloader
{
    GObject parent_instance;
};

GtResourceDownloader* gt_resource_downloader_new();
GtResourceDownloader* gt_resource_downloader_new_with_cache(const gchar* filepath);
void                  gt_resource_downloader_set_image_filetype(GtResourceDownloader* self, const gchar* filetype);
GdkPixbuf*            gt_resource_downloader_download_image(GtResourceDownloader* self, const gchar* uri, const gchar* name, GError** error);
void                  gt_resource_downloader_download_image_async(GtResourceDownloader* self, const gchar* uri, const gchar* name, GAsyncReadyCallback cb, GCancellable* cancel, gpointer udata);
GdkPixbuf*            gt_resource_donwloader_download_image_finish(GtResourceDownloader* self, GAsyncResult* result, GError** error);
GdkPixbuf*            gt_resource_downloader_download_image_immediately(GtResourceDownloader* self, const gchar* uri, const gchar* name, ResourceDownloaderFunc cb, gpointer udata, GError** error);

G_END_DECLS

#endif
