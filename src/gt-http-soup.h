#ifndef GT_HTTP_SOUP_H
#define GT_HTTP_SOUP_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GT_TYPE_HTTP_SOUP gt_http_soup_get_type()

G_DECLARE_FINAL_TYPE(GtHTTPSoup, gt_http_soup, GT, HTTP_SOUP, GObject);

struct _GtHTTPSoup
{
    GObject parent_instance;
};

GtHTTPSoup* gt_http_soup_new();

G_END_DECLS

#endif
