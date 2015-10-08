#ifndef GT_CHANNELS_CONTAINER_H
#define GT_CHANNELS_CONTAINER_H

#include <gtk/gtk.h>
#include "gt-channels-container-child.h"

G_BEGIN_DECLS

#define GT_TYPE_CHANNELS_CONTAINER (gt_channels_container_get_type())

G_DECLARE_DERIVABLE_TYPE(GtChannelsContainer, gt_channels_container, GT, CHANNELS_CONTAINER, GtkBox)

typedef enum
{
    GT_CHANNELS_CONTAINER_TYPE_TOP,
    GT_CHANNELS_CONTAINER_TYPE_FAVOURITE,
    GT_CHANNELS_CONTAINER_TYPE_SEARCH,
    GT_CHANNELS_CONTAINER_TYPE_GAME
} GtChannelsContainerType;

struct _GtChannelsContainerClass
{
    GtkBoxClass parent_class;

    void (*show_load_spinner) (GtChannelsContainer* self, gboolean show);
    void (*append_channels) (GtChannelsContainer* self, GList* channels);
    GtkFlowBox* (*get_channels_flow) (GtChannelsContainer* self);

    void (*bottom_edge_reached) (GtChannelsContainer* self);
    void (*refresh) (GtChannelsContainer* self);
    void (*filter) (GtChannelsContainer* self, const gchar* query);
};

GtChannelsContainer* gt_channels_container_new();

void gt_channels_container_refresh(GtChannelsContainer* self);
void gt_channels_container_set_filter_query(GtChannelsContainer* self, const gchar* query);

G_END_DECLS

#endif
