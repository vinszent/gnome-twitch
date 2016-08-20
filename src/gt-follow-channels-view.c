#include "gt-follow-channels-view.h"

typedef struct
{
    void* tmp;
} GtFollowChannelsViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtFollowChannelsView, gt_follow_channels_view, GTK_TYPE_BOX)

enum
{
    PROP_0,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtFollowChannelsView*
gt_follow_channels_view_new(void)
{
    return g_object_new(GT_TYPE_FOLLOW_CHANNELS_VIEW,
                        NULL);
}

static void
finalize(GObject* object)
{
    GtFollowChannelsView* self = (GtFollowChannelsView*) object;
    GtFollowChannelsViewPrivate* priv = gt_follow_channels_view_get_instance_private(self);

    G_OBJECT_CLASS(gt_follow_channels_view_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtFollowChannelsView* self = GT_FOLLOW_CHANNELS_VIEW(obj);
    GtFollowChannelsViewPrivate* priv = gt_follow_channels_view_get_instance_private(self);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
set_property(GObject*      obj,
             guint         prop,
             const GValue* val,
             GParamSpec*   pspec)
{
    GtFollowChannelsView* self = GT_FOLLOW_CHANNELS_VIEW(obj);
    GtFollowChannelsViewPrivate* priv = gt_follow_channels_view_get_instance_private(self);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_follow_channels_view_class_init(GtFollowChannelsViewClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), "/com/vinszent/GnomeTwitch/ui/gt-follow-channels-view.ui");
}

static void
gt_follow_channels_view_init(GtFollowChannelsView* self)
{
    gtk_widget_init_template(GTK_WIDGET(self));
}
