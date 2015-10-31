#ifndef GT_PLAYER_CLUTTER_H
#define GT_PLAYER_CLUTTER_H

#include <gtk/gtk.h>
#include <clutter-gtk/clutter-gtk.h>
#include <clutter-gst/clutter-gst.h>

G_BEGIN_DECLS

#define GT_TYPE_PLAYER_CLUTTER (gt_player_clutter_get_type())

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtkClutterEmbed, g_object_unref)

G_DECLARE_FINAL_TYPE(GtPlayerClutter, gt_player_clutter, GT, PLAYER_CLUTTER, GtkClutterEmbed)

struct _GtPlayerClutter
{
    GtkClutterEmbed parent_instance;

    GSimpleActionGroup* action_group;
};

GtPlayerClutter* gt_player_clutter_new(void);

G_END_DECLS

#endif
