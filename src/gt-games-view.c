#include "gt-games-view.h"
#include "gt-games-view-child.h"
#include "gt-app.h"
#include "gt-twitch-game.h"
#include "gt-win.h"
#include "utils.h"
#include <string.h>

#define MAX_QUERY 50 

typedef struct
{
    GtkWidget* games_scroll;
    GtkWidget* games_flow;
    GtkWidget* search_bar;
    GtkWidget* spinner_revealer;

    gint games_page;

    gchar* search_query;
    gint search_page;

    GCancellable* cancel;
} GtGamesViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtGamesView, gt_games_view, GTK_TYPE_BOX)

static void
top_games_cb(GObject* source,
             GAsyncResult* res,
             gpointer udata);
static void
search_games_cb(GObject* source,
                GAsyncResult* res,
                gpointer udata);

enum 
{
    PROP_0,
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
edge_reached_cb(GtkScrolledWindow* scroll,
                GtkPositionType pos,
                gpointer udata)
{
    GtGamesView* self = GT_GAMES_VIEW(udata);
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);

    if (pos == GTK_POS_BOTTOM)
    {
        if (strlen(priv->search_query) == 0)
        {
            gt_twitch_top_games_async(main_app->twitch,
                                      MAX_QUERY,
                                      ++priv->games_page*MAX_QUERY,
                                      priv->cancel,
                                      (GAsyncReadyCallback) top_games_cb,
                                      self);
        }
        else
        {
            gt_twitch_search_games_async(main_app->twitch,
                                         priv->search_query,
                                         MAX_QUERY,
                                         ++priv->search_page*MAX_QUERY,
                                         priv->cancel,
                                         (GAsyncReadyCallback) search_games_cb,
                                         self);
        }
    }

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->spinner_revealer), TRUE);
}

static void
top_games_cb(GObject* source,
             GAsyncResult* res,
             gpointer udata)
{
    GtGamesView* self = GT_GAMES_VIEW(udata);
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);
    GError* error = NULL;

    GList* new = g_task_propagate_pointer(G_TASK(res), &error);

    if (error)
    {
        g_error_free(error);
        return;
    }

    gt_games_view_append_games(self, new);
    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->spinner_revealer), FALSE);
}

static void
search_games_cb(GObject* source,
                GAsyncResult* res,
                gpointer udata)
{
    GtGamesView* self = GT_GAMES_VIEW(udata);
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);
    GError* error = NULL;

    GList* new = g_task_propagate_pointer(G_TASK(res), &error);

    if (error)
    {
        g_error_free(error);
        return;
    }

    gt_games_view_append_games(self, new);
    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->spinner_revealer), FALSE);
}


static void
child_activated_cb(GtkFlowBox* flow,
                   GtkFlowBoxChild* child,
                   gpointer udata)
{
    GtGamesViewChild* chan = GT_GAMES_VIEW_CHILD(child);
    GtTwitchGame* game;
    GtWin* win;

    gt_games_view_child_hide_overlay(chan); //FIXME: Doesn't work

    win = GT_WIN(gtk_widget_get_toplevel(GTK_WIDGET(flow)));

    g_object_get(chan, "twitch-game", &game, NULL);

    gt_channels_view_show_game_channels(gt_win_get_channels_view(win),
                                      game);


    gt_win_browse_channels_view(win);

    g_object_unref(game);
}

static void
search_entry_cb(GtkSearchEntry* entry,
                gpointer udata)
{
    GtGamesView* self = GT_GAMES_VIEW(udata);
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);

    g_cancellable_cancel(priv->cancel);
    if (priv->cancel)
        g_object_unref(priv->cancel);
    priv->cancel = g_cancellable_new();

    priv->search_query = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    priv->search_page = 0;

    if (strlen(priv->search_query) == 0)
    {
        priv->games_page = 0;
        priv->search_page = 0;
        gt_twitch_top_games_async(main_app->twitch,
                                  MAX_QUERY,
                                  priv->games_page*MAX_QUERY,
                                  priv->cancel,
                                  (GAsyncReadyCallback) top_games_cb,
                                  self);


    }
    else
    {
        gt_twitch_search_games_async(main_app->twitch,
                                     priv->search_query,
                                     MAX_QUERY,
                                     priv->search_page*MAX_QUERY,
                                     priv->cancel,
                                     (GAsyncReadyCallback) search_games_cb,
                                     self);
    }

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->spinner_revealer), TRUE);

    gtk_container_clear(GTK_CONTAINER(priv->games_flow));
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
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_games_view_class_init(GtGamesViewClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), 
                                                "/com/gnome-twitch/ui/gt-games-view.ui");
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtGamesView, games_scroll);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtGamesView, games_flow);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtGamesView, search_bar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), GtGamesView, spinner_revealer);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), search_entry_cb);
}

static void
gt_games_view_init(GtGamesView* self)
{
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);

    gtk_widget_init_template(GTK_WIDGET(self));

    priv->games_page = 0;
    priv->search_page = 0;
    priv->search_query = "";

    g_signal_connect(priv->games_flow, "child-activated", G_CALLBACK(child_activated_cb), self);
    g_signal_connect(priv->games_scroll, "edge-reached", G_CALLBACK(edge_reached_cb), self);

    gt_twitch_top_games_async(main_app->twitch,
                              MAX_QUERY,
                              priv->games_page,
                              priv->cancel,
                              (GAsyncReadyCallback) top_games_cb,
                              self);
}

void
gt_games_view_append_games(GtGamesView* self, GList* games)
{
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);

    for (GList* l = games; l != NULL; l = l->next)
    {
        GtGamesViewChild* child = gt_games_view_child_new(GT_TWITCH_GAME(l->data));
        gtk_widget_show_all(GTK_WIDGET(child));
        gtk_container_add(GTK_CONTAINER(priv->games_flow), GTK_WIDGET(child));
    }
}

void
gt_games_view_start_search(GtGamesView* self)
{
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);

    gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(priv->search_bar), TRUE);
}

void
gt_games_view_stop_search(GtGamesView* self)
{
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);

    gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(priv->search_bar), FALSE);
}

void
gt_games_view_refresh(GtGamesView* self)
{
    GtGamesViewPrivate* priv = gt_games_view_get_instance_private(self);

    g_cancellable_cancel(priv->cancel);
    if (priv->cancel)
        g_object_unref(priv->cancel);
    priv->cancel = g_cancellable_new();

    if (strlen(priv->search_query) == 0)
    {
        priv->games_page = 0;
        priv->search_page = 0;
        gt_twitch_top_games_async(main_app->twitch,
                                  MAX_QUERY,
                                  priv->games_page*MAX_QUERY,
                                  priv->cancel,
                                  (GAsyncReadyCallback) top_games_cb,
                                  self);


    }
    else
    {
        priv->search_page = 0;
        gt_twitch_search_games_async(main_app->twitch,
                                     priv->search_query,
                                     MAX_QUERY,
                                     priv->search_page*MAX_QUERY,
                                     priv->cancel,
                                     (GAsyncReadyCallback) search_games_cb,
                                     self);
    }

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->spinner_revealer), TRUE);

    gtk_container_clear(GTK_CONTAINER(priv->games_flow));
}
