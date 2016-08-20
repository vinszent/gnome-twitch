#ifndef GT_FOLLOWS_MANAGER_H
#define GT_FOLLOWS_MANAGER_H

#include <gtk/gtk.h>

#include "gt-channel.h"

G_BEGIN_DECLS

#define GT_TYPE_FOLLOWS_MANAGER (gt_follows_manager_get_type())

G_DECLARE_FINAL_TYPE(GtFollowsManager, gt_follows_manager, GT, FOLLOWS_MANAGER, GObject)

struct _GtFollowsManager
{
    GObject parent_instance;

    GList* follow_channels;
};

GtFollowsManager* gt_follows_manager_new(void);
void gt_follows_manager_load_from_file(GtFollowsManager* self);
void gt_follows_manager_load_from_twitch(GtFollowsManager* self);
void gt_follows_manager_save(GtFollowsManager* self);
gboolean gt_follows_manager_is_channel_followed(GtFollowsManager* self, GtChannel* chan);
void gt_follows_manager_attach_to_channel(GtFollowsManager* self, GtChannel* chan);


G_END_DECLS

#endif
