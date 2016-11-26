#ifndef GT_ITEM_CONTAINER_H
#define GT_ITEM_CONTAINER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GT_TYPE_ITEM_CONTAINER (gt_item_container_get_type())

G_DECLARE_DERIVABLE_TYPE(GtItemContainer, gt_item_container, GT, ITEM_CONTAINER, GtkStack)

struct _GtItemContainerClass
{
    GtkStackClass parent_class;

    /* Protected funcs */
    void (*get_properties) (GtItemContainer* item_container, gint* child_width, gint* child_height, gboolean* append_extra);
    GTaskThreadFunc fetch_items;
    GtkWidget* (*create_child) (gpointer data);
    void (*activate_child) (GtItemContainer* item_container, gpointer child);
};

typedef struct
{
    uint amount;
    uint offset;
} FetchItemsData;


GtkWidget* gt_item_container_get_flow_box(GtItemContainer* self);
void gt_item_container_append_items(GtItemContainer* self, GList* items);
void gt_item_container_refresh(GtItemContainer* self);

G_END_DECLS;

#endif
