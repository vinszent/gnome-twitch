#ifndef GT_SEARCH_CHANNEL_CONTAINER_H
#define GT_SEARCH_CHANNEL_CONTAINER_H

#include "gt-item-container.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define GT_TYPE_SEARCH_CHANNEL_CONTAINER gt_search_channel_container_get_type()

G_DECLARE_FINAL_TYPE(GtSearchChannelContainer, gt_search_channel_container, GT, SEARCH_CHANNEL_CONTAINER, GtItemContainer);

struct _GtSearchChannelContainer
{
    GtItemContainer parent_instance;
};

GtSearchChannelContainer* gt_search_channel_container_new();
void gt_search_channel_container_set_query(GtSearchChannelContainer* self, const gchar* query);
const gchar* gt_search_channel_container_get_query(GtSearchChannelContainer* self);

G_END_DECLS

#endif
