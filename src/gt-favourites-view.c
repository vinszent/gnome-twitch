#include "gt-favourites-view.h"
#include "gt-channels-container-favourite.h"

typedef struct
{
    gboolean search_active;

    GtkWidget* favourite_container;
    GtkWidget* search_bar;
} GtFavouritesViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtFavouritesView, gt_favourites_view, GTK_TYPE_BOX)

enum 
{
    PROP_0,
    PROP_SEARCH_ACTIVE,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtFavouritesView*
gt_favourites_view_new(void)
{
    return g_object_new(GT_TYPE_FAVOURITES_VIEW, 
                        NULL);
}

static void
search_changed_cb(GtkEditable* edit,
                  gpointer udata)
{
    GtFavouritesView* self = GT_FAVOURITES_VIEW(udata);
    GtFavouritesViewPrivate* priv = gt_favourites_view_get_instance_private(self);

    const gchar* query = gtk_entry_get_text(GTK_ENTRY(edit));

    gt_channels_container_set_filter_query(GT_CHANNELS_CONTAINER(priv->favourite_container), query);
}

static void
finalize(GObject* object)
{
    GtFavouritesView* self = (GtFavouritesView*) object;
    GtFavouritesViewPrivate* priv = gt_favourites_view_get_instance_private(self);

    G_OBJECT_CLASS(gt_favourites_view_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtFavouritesView* self = GT_FAVOURITES_VIEW(obj);
    GtFavouritesViewPrivate* priv = gt_favourites_view_get_instance_private(self);

    switch (prop)
    {
        case PROP_SEARCH_ACTIVE:
            g_value_set_boolean(val, priv->search_active);
            break;
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
    GtFavouritesView* self = GT_FAVOURITES_VIEW(obj);
    GtFavouritesViewPrivate* priv = gt_favourites_view_get_instance_private(self);

    switch (prop)
    {
        case PROP_SEARCH_ACTIVE:
            priv->search_active = g_value_get_boolean(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_favourites_view_class_init(GtFavouritesViewClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    GT_TYPE_CHANNELS_CONTAINER_FAVOURITE;

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_SEARCH_ACTIVE] = g_param_spec_boolean("search-active",
                                                     "Search Active",
                                                     "Whether search is active",
                                                     FALSE,
                                                     G_PARAM_READWRITE);

    g_object_class_install_properties(object_class,
                                      NUM_PROPS,
                                      props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), "/com/gnome-twitch/ui/gt-favourites-view.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtFavouritesView, favourite_container);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtFavouritesView, search_bar);

    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), search_changed_cb);
}

static void
gt_favourites_view_init(GtFavouritesView* self)
{
    GtFavouritesViewPrivate* priv = gt_favourites_view_get_instance_private(self);

    gtk_widget_init_template(GTK_WIDGET(self));

    g_object_bind_property(self, "search-active",
                           priv->search_bar, "search-mode-enabled",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
}
