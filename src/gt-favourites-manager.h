#ifndef GT_FAVOURITES_MANAGER_H
#define GT_FAVOURITES_MANAGER_H

#include <gtk/gtk.h>

#include "gt-channel.h"

G_BEGIN_DECLS

#define GT_TYPE_FAVOURITES_MANAGER (gt_favourites_manager_get_type())

G_DECLARE_FINAL_TYPE(GtFavouritesManager, gt_favourites_manager, GT, FAVOURITES_MANAGER, GObject)

struct _GtFavouritesManager
{
    GObject parent_instance;

    GList* favourite_channels;
};

GtFavouritesManager* gt_favourites_manager_new(void);
void gt_favourites_manager_load(GtFavouritesManager* self);
void gt_favourites_manager_save(GtFavouritesManager* self);
gboolean gt_favourites_manager_is_channel_favourited(GtFavouritesManager* self, GtChannel* chan);
void gt_favourites_manager_attach_to_channel(GtFavouritesManager* self, GtChannel* chan);
void gt_favourites_manager_update(GtFavouritesManager* self);

G_END_DECLS

#endif
