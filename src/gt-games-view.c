#include "gt-games-view.h"
#include "gt-games-container.h"
#include "gt-games-container-top.h"
#include "gt-games-container-search.h"

#define VISIBLE_CONTAINER GT_GAMES_CONTAINER(gtk_stack_get_visible_child(GTK_STACK(priv->games_stack)))

typedef struct
{
    gboolean search_active;
    
    GtkWidget* games_stack;
    GtkWidget* search_bar;
    GtkWidget* top_container;
    GtkWidget* search_container;
} GtGamesViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtGamesView, gt_games_view, GTK_TYPE_BOX)

enum 
{
    PROP_0,
    PROP_SEARCH_ACTIVE,
    PROP_SHOWING_TOP_GAMES,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtGamesView*
gt_games_view_new(void)
{
    return g_object_new(GT_TYPE_GAMES_VIEW, 
                        NULL);
}

static void
search_changed_cb(GtkEditable* edit,
                  gpointer udata)
{
    GtGamesView* self = GT_GAMES_VIEW(udata);
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);

    const gchar* query = gtk_entry_get_text(GTK_ENTRY(edit));

    gt_games_container_set_filter_query(VISIBLE_CONTAINER, query);
}

static void
search_active_cb(GObject* source,
                 GParamSpec* pspec,
                 gpointer udata)
{
    GtGamesView* self = GT_GAMES_VIEW(udata);
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);

    if (priv->search_active)
        gtk_stack_set_visible_child(GTK_STACK(priv->games_stack), priv->search_container);
    else
        gtk_stack_set_visible_child(GTK_STACK(priv->games_stack), priv->top_container);
}

static void
finalize(GObject* object)
{
    GtGamesView* self = (GtGamesView*) object;
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);

    G_OBJECT_CLASS(gt_games_view_parent_class)->finalize(object);
}

static void
get_property (GObject*    obj,
              guint       prop,
              GValue*     val,
              GParamSpec* pspec)
{
    GtGamesView* self = GT_GAMES_VIEW(obj);
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);

    switch (prop)
    {
        case PROP_SEARCH_ACTIVE:
            g_value_set_boolean(val, priv->search_active);
            break;
        case PROP_SHOWING_TOP_GAMES:
            g_value_set_boolean(val, gtk_stack_get_visible_child(GTK_STACK(priv->games_stack)) == priv->top_container);
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
    GtGamesView* self = GT_GAMES_VIEW(obj);
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);

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
gt_games_view_class_init(GtGamesViewClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    GT_TYPE_GAMES_CONTAINER_TOP;
    GT_TYPE_GAMES_CONTAINER_SEARCH;

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    props[PROP_SEARCH_ACTIVE] = g_param_spec_boolean("search-active",
                                                     "Search Active",
                                                     "Whether search is active",
                                                     FALSE,
                                                     G_PARAM_READWRITE);
    props[PROP_SHOWING_TOP_GAMES] = g_param_spec_boolean("showing-top-games",
                                                         "Showing Top Games",
                                                         "Whether showing top games",
                                                         FALSE,
                                                         G_PARAM_READABLE);
    g_object_class_install_properties(object_class,
                                      NUM_PROPS,
                                      props);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), "/com/gnome-twitch/ui/gt-games-view.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtGamesView, games_stack);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtGamesView, search_bar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtGamesView, top_container);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtGamesView, search_container);

    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), search_changed_cb);
}

static void
gt_games_view_init(GtGamesView* self)
{
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);

    gtk_widget_init_template(GTK_WIDGET(self));

    g_object_bind_property(self, "search-active",
                           priv->search_bar, "search-mode-enabled",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_signal_connect(self, "notify::search-active", G_CALLBACK(search_active_cb), self);
}

void
gt_games_view_refresh(GtGamesView* self)
{
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);

    gt_games_container_refresh(VISIBLE_CONTAINER);
}
