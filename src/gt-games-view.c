#include "gt-games-view.h"
#include "gt-games-container.h"
#include "gt-games-container-top.h"
#include "gt-games-container-search.h"
#include "gt-channels-container-game.h"
#include "gt-game.h"

#define VISIBLE_CHILD gtk_stack_get_visible_child(GTK_STACK(priv->games_stack))
#define VISIBLE_CONTAINER GT_GAMES_CONTAINER(VISIBLE_CHILD)

typedef struct
{
    gboolean search_active;
    
    GtkWidget* games_stack;
    GtkWidget* search_bar;
    GtkWidget* top_container;
    GtkWidget* search_container;
    GtkWidget* game_container;

    gboolean was_showing_game;
} GtGamesViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtGamesView, gt_games_view, GTK_TYPE_BOX)

enum 
{
    PROP_0,
    PROP_SEARCH_ACTIVE,
    PROP_SHOWING_TOP_GAMES,
    PROP_SHOWING_GAME_CHANNELS,
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

    /* if (VISIBLE_CHILD == priv->search_container) */
    gt_games_container_set_filter_query(GT_GAMES_CONTAINER(priv->search_container), query);
}

static void
search_active_cb(GObject* source,
                 GParamSpec* pspec,
                 gpointer udata)
{
    GtGamesView* self = GT_GAMES_VIEW(udata);
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);

    if (priv->search_active)
        gt_games_view_show_type(self, GT_GAMES_CONTAINER_TYPE_SEARCH);
    else if (priv->was_showing_game)
        gt_games_view_show_type(self, GT_CHANNELS_CONTAINER_TYPE_GAME);
    else
        gt_games_view_show_type(self, GT_GAMES_CONTAINER_TYPE_TOP);
}

static void
game_activated_cb(GtGamesContainer* container,
                  GtGame* game,
                  gpointer udata)
{
    GtGamesView* self = GT_GAMES_VIEW(udata);
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);
    gchar* name;

    g_object_get(game, "name", &name, NULL);

    gt_channels_container_set_filter_query(GT_CHANNELS_CONTAINER(priv->game_container), name);

    gt_games_view_show_type(self, GT_CHANNELS_CONTAINER_TYPE_GAME);
    
    g_signal_handlers_block_by_func(self, search_active_cb, self);
    g_object_set(self, "search-active", FALSE, NULL);
    g_signal_handlers_unblock_by_func(self, search_active_cb, self);

    g_free(name);
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
        case PROP_SHOWING_GAME_CHANNELS:
            g_value_set_boolean(val, gtk_stack_get_visible_child(GTK_STACK(priv->games_stack)) == priv->game_container);
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
    GT_TYPE_CHANNELS_CONTAINER_GAME;

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
    props[PROP_SHOWING_GAME_CHANNELS] = g_param_spec_boolean("showing-game-channels",
                                                             "Showing Game Channels",
                                                             "Whether showing game channels",
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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtGamesView, game_container);

    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), search_changed_cb);
}

static void
gt_games_view_init(GtGamesView* self)
{
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);

    priv->was_showing_game = FALSE;

    gtk_widget_init_template(GTK_WIDGET(self));

    g_object_bind_property(self, "search-active",
                           priv->search_bar, "search-mode-enabled",
                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
    g_signal_connect(self, "notify::search-active", G_CALLBACK(search_active_cb), self);
    g_signal_connect(priv->top_container, "game-activated", G_CALLBACK(game_activated_cb), self);
    g_signal_connect(priv->search_container, "game-activated", G_CALLBACK(game_activated_cb), self);
}

void
gt_games_view_refresh(GtGamesView* self)
{
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);

    if (GT_IS_CHANNELS_CONTAINER(VISIBLE_CHILD))
        gt_channels_container_refresh(GT_CHANNELS_CONTAINER(VISIBLE_CHILD));
    else
        gt_games_container_refresh(VISIBLE_CONTAINER);
}

void
gt_games_view_show_type(GtGamesView* self, gint type)
{
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);

    switch (type)
    {
        case GT_GAMES_CONTAINER_TYPE_TOP:
            gtk_stack_set_visible_child(GTK_STACK(priv->games_stack), priv->top_container);
            priv->was_showing_game = FALSE;
            break;
        case GT_GAMES_CONTAINER_TYPE_SEARCH:
            gtk_stack_set_visible_child(GTK_STACK(priv->games_stack), priv->search_container);
            break;
        case GT_CHANNELS_CONTAINER_TYPE_GAME:
            gtk_stack_set_visible_child(GTK_STACK(priv->games_stack), priv->game_container);
            priv->was_showing_game = TRUE;
            break;
        default:
            break;
    }

    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_SHOWING_TOP_GAMES]);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_SHOWING_GAME_CHANNELS]);
}

gboolean
gt_games_view_handle_event(GtGamesView* self, GdkEvent* event)
{
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);

    return gtk_search_bar_handle_event(GTK_SEARCH_BAR(priv->search_bar), event);
}
