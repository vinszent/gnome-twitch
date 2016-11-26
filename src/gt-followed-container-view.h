#ifndef GT_FOLLOWED_CONTAINER_VIEW_H
#define GT_FOLLOWED_CONTAINER_VIEW_H

#include <glib-object.h>
#include "gt-container-view.h"

G_BEGIN_DECLS

#define GT_TYPE_FOLLOWED_CONTAINER_VIEW gt_followed_container_view_get_type()

G_DECLARE_FINAL_TYPE(GtFollowedContainerView, gt_followed_container_view, GT, FOLLOWED_CONTAINER_VIEW, GtContainerView);

struct _GtFollowedContainerView
{
    GtContainerView parent_instance;
};

G_END_DECLS

#endif
