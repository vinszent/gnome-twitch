#include "gt-favourite-channels-view.h"

typedef struct
{
    void* tmp;
} GtFavouriteChannelsViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtFavouriteChannelsView, gt_favourite_channels_view, GTK_TYPE_BOX)

enum
{
    PROP_0,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtFavouriteChannelsView*
gt_favourite_channels_view_new(void)
{
    return g_object_new(GT_TYPE_FAVOURITE_CHANNELS_VIEW,
                        NULL);
}

static void
finalize(GObject* object)
{
    GtFavouriteChannelsView* self = (GtFavouriteChannelsView*) object;
    GtFavouriteChannelsViewPrivate* priv = gt_favourite_channels_view_get_instance_private(self);

    G_OBJECT_CLASS(gt_favourite_channels_view_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtFavouriteChannelsView* self = GT_FAVOURITE_CHANNELS_VIEW(obj);
    GtFavouriteChannelsViewPrivate* priv = gt_favourite_channels_view_get_instance_private(self);

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
    GtFavouriteChannelsView* self = GT_FAVOURITE_CHANNELS_VIEW(obj);
    GtFavouriteChannelsViewPrivate* priv = gt_favourite_channels_view_get_instance_private(self);

    switch (prop)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_favourite_channels_view_class_init(GtFavouriteChannelsViewClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), "/com/vinszent/GnomeTwitch/ui/gt-favourite-channels-view.ui");
}

static void
gt_favourite_channels_view_init(GtFavouriteChannelsView* self)
{
    gtk_widget_init_template(GTK_WIDGET(self));
}
