#include "gt-player-mpv.h"
#include "gt-twitch-chat-view.h"
#include "gt-channel.h"
#include <mpv/client.h>
#include <mpv/opengl_cb.h>
#include <epoxy/gl.h>
#include <gdk/gdk.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#include <epoxy/glx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#include <epoxy/egl.h>
#endif

typedef struct
{
    mpv_handle* mpv;
    mpv_opengl_cb_context* mpv_opengl;

    GtkWidget* opengl_area;
    GtkWidget* chat_view;

    gchar* current_uri;
    gboolean opengl_ready;

    gdouble volume;
    gboolean playing;
    gdouble chat_opacity;
    gdouble chat_width;
    gdouble chat_height;
    gdouble chat_x;
    gdouble chat_y;
    gboolean chat_docked;
    gboolean chat_visible;

    GtChannel* open_channel;
} GtPlayerMpvPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtPlayerMpv, gt_player_mpv, GT_TYPE_PLAYER)

enum
{
    PROP_0,
    PROP_VOLUME,
    PROP_OPEN_CHANNEL,
    PROP_PLAYING,
    PROP_CHAT_OPACITY,
    PROP_CHAT_WIDTH,
    PROP_CHAT_HEIGHT,
    PROP_CHAT_DOCKED,
    PROP_CHAT_X,
    PROP_CHAT_Y,
    PROP_CHAT_VISIBLE,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtPlayerMpv*
gt_player_mpv_new()
{
    return g_object_new(GT_TYPE_PLAYER_MPV,
                        NULL);
}

static void
finalise(GObject* obj)
{
    GtPlayerMpv* self = GT_PLAYER_MPV(obj);
    GtPlayerMpvPrivate* priv = gt_player_mpv_get_instance_private(self);

    gtk_gl_area_make_current(GTK_GL_AREA(priv->opengl_area));
    mpv_opengl_cb_uninit_gl(priv->mpv_opengl);

    G_OBJECT_CLASS(gt_player_mpv_parent_class)->finalize(obj);
}

static void
get_property(GObject* obj,
             guint prop,
             GValue* val,
             GParamSpec* pspec)
{
    GtPlayerMpv* self = GT_PLAYER_MPV(obj);
    GtPlayerMpvPrivate* priv = gt_player_mpv_get_instance_private(self);

    switch (prop)
    {
        case PROP_VOLUME:
            g_value_set_double(val, priv->volume);
            break;
        case PROP_OPEN_CHANNEL:
            g_value_set_object(val, priv->open_channel);
            break;
        case PROP_PLAYING:
            g_value_set_boolean(val, priv->playing);
            break;
        case PROP_CHAT_WIDTH:
            g_value_set_double(val, priv->chat_width);
            break;
        case PROP_CHAT_HEIGHT:
            g_value_set_double(val, priv->chat_height);
            break;
        case PROP_CHAT_DOCKED:
            g_value_set_boolean(val, priv->chat_docked);
            break;
        case PROP_CHAT_X:
            g_value_set_double(val, priv->chat_x);
            break;
        case PROP_CHAT_Y:
            g_value_set_double(val, priv->chat_y);
            break;
        case PROP_CHAT_VISIBLE:
            g_value_set_boolean(val, priv->chat_visible);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
set_property(GObject* obj,
             guint prop,
             const GValue* val,
             GParamSpec* pspec)
{
    GtPlayerMpv* self = GT_PLAYER_MPV(obj);
    GtPlayerMpvPrivate* priv = gt_player_mpv_get_instance_private(self);

    switch (prop)
    {
        case PROP_VOLUME:
            priv->volume = g_value_get_double(val);
            break;
        case PROP_OPEN_CHANNEL:
            g_clear_object(&priv->open_channel);
            priv->open_channel = g_value_dup_object(val);
            break;
        case PROP_CHAT_WIDTH:
            priv->chat_width = g_value_get_double(val);
            break;
        case PROP_CHAT_HEIGHT:
            priv->chat_height = g_value_get_double(val);
            break;
        case PROP_CHAT_DOCKED:
            priv->chat_docked = g_value_get_boolean(val);
            break;
        case PROP_CHAT_X:
            priv->chat_x = g_value_get_double(val);
            break;
        case PROP_CHAT_Y:
            priv->chat_y = g_value_get_double(val);
            break;
        case PROP_CHAT_VISIBLE:
            priv->chat_visible = g_value_get_boolean(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static inline void
check_mpv_error(int status)
{
    if (status < 0)
        g_error("{GtPlayerMpv} Mpv error %s\n", mpv_error_string(status));
}

static void*
get_proc_address(void* ctx, const gchar* name)
{
    GdkDisplay* display = gdk_display_get_default();

    #ifdef GDK_WINDOWING_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY(display))
        return eglGetProcAddress(name);
    #endif
    #ifdef GDK_WINDOWING_X11
    if (GDK_IS_X11_DISPLAY(display))
        return (void*)(intptr_t) glXGetProcAddressARB((const GLubyte*) name);
    #endif

    g_return_val_if_reached(NULL);
}

static void
opengl_cb(gpointer udata)
{
    GtPlayerMpv* self = GT_PLAYER_MPV(udata);
    GtPlayerMpvPrivate* priv = gt_player_mpv_get_instance_private(self);

    gtk_gl_area_queue_render(GTK_GL_AREA(priv->opengl_area));
}

static gboolean
render_cb(GtkGLArea* area,
          GdkGLContext* ctxt,
          gpointer udata)
{
    GtPlayerMpv* self = GT_PLAYER_MPV(udata);
    GtPlayerMpvPrivate* priv = gt_player_mpv_get_instance_private(self);

    int width;
    int height;
    int fbo;

    if (!priv->opengl_ready)
    {
        check_mpv_error(mpv_opengl_cb_init_gl(priv->mpv_opengl, NULL,
                                              get_proc_address, NULL));
        priv->opengl_ready = TRUE;
    }

    width = gtk_widget_get_allocated_height(priv->opengl_area);
    height = (-1)*gtk_widget_get_allocated_width(priv->opengl_area);
    fbo = -1;

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
    mpv_opengl_cb_draw(priv->mpv_opengl, fbo, width, height);

    return TRUE;
}

static void
set_uri(GtPlayer* player, const gchar* uri)
{
    GtPlayerMpv* self = GT_PLAYER_MPV(player);
    GtPlayerMpvPrivate* priv = gt_player_mpv_get_instance_private(self);

    g_free(priv->current_uri);
    priv->current_uri = g_strdup(uri);
}

static void
play(GtPlayer* player)
{
    GtPlayerMpv* self = GT_PLAYER_MPV(player);
    GtPlayerMpvPrivate* priv = gt_player_mpv_get_instance_private(self);

    const gchar* mpv_cmd[] = {"loadfile", priv->current_uri, "replace", NULL};

    check_mpv_error(mpv_command(priv->mpv, mpv_cmd));

    priv->playing = TRUE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_PLAYING]);
}

static void
stop(GtPlayer* player)
{
    GtPlayerMpv* self = GT_PLAYER_MPV(player);
    GtPlayerMpvPrivate* priv = gt_player_mpv_get_instance_private(self);

    const gchar* mpv_cmd[] = {"stop", NULL};

    check_mpv_error(mpv_command(priv->mpv, mpv_cmd));

    priv->playing = FALSE;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_PLAYING]);
}

static GtkWidget*
get_chat_view(GtPlayer* player)
{
    GtPlayerMpv* self = GT_PLAYER_MPV(player);
    GtPlayerMpvPrivate* priv = gt_player_mpv_get_instance_private(self);

    return priv->chat_view;
}

static void
gt_player_mpv_class_init(GtPlayerMpvClass* klass)
{
    GObjectClass* obj_class = G_OBJECT_CLASS(klass);
    GtPlayerClass* player_class = GT_PLAYER_CLASS(klass);

    obj_class->finalize = finalise;
    obj_class->get_property = get_property;
    obj_class->set_property = set_property;

    player_class->set_uri = set_uri;
    player_class->play = play;
    player_class->stop = stop;
    player_class->get_chat_view = get_chat_view;
    props[PROP_VOLUME] = g_param_spec_double("volume",
                                             "Volume",
                                             "Volume of player",
                                             0.0, 1.0, 0.3,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_OPEN_CHANNEL] = g_param_spec_object("open-channel",
                                                   "Open Channel",
                                                   "Currently open channel",
                                                   GT_TYPE_CHANNEL,
                                                   G_PARAM_READWRITE);
    props[PROP_PLAYING] = g_param_spec_boolean("playing",
                                               "Playing",
                                               "Whether playing",
                                               FALSE,
                                               G_PARAM_READABLE);
    props[PROP_CHAT_WIDTH] = g_param_spec_double("chat-width",
                                                 "Chat Width",
                                                 "Current chat width",
                                                 0, 1.0, 0.2,
                                                 G_PARAM_READWRITE);
    props[PROP_CHAT_HEIGHT] = g_param_spec_double("chat-height",
                                                  "Chat Height",
                                                  "Current chat height",
                                                  0, 1.0, 1.0,
                                                  G_PARAM_READWRITE);
    props[PROP_CHAT_DOCKED] = g_param_spec_boolean("chat-docked",
                                                   "Chat Docked",
                                                   "Whether chat docked",
                                                   TRUE,
                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_CHAT_X] = g_param_spec_double("chat-x",
                                             "Chat X",
                                             "Current chat x",
                                             0, G_MAXDOUBLE, 0,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_CHAT_Y] = g_param_spec_double("chat-y",
                                             "Chat Y",
                                             "Current chat y",
                                             0, G_MAXDOUBLE, 0.0,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_CHAT_VISIBLE] = g_param_spec_boolean("chat-visible",
                                                    "Chat Visible",
                                                    "Whether chat visible",
                                                    TRUE,
                                                    G_PARAM_READWRITE);

    g_object_class_override_property(obj_class, PROP_VOLUME, "volume");
    g_object_class_override_property(obj_class, PROP_OPEN_CHANNEL, "open-channel");
    g_object_class_override_property(obj_class, PROP_PLAYING, "playing");
    g_object_class_override_property(obj_class, PROP_CHAT_DOCKED, "chat-docked");
    g_object_class_override_property(obj_class, PROP_CHAT_VISIBLE, "chat-visible");
    g_object_class_override_property(obj_class, PROP_CHAT_X, "chat-x");
    g_object_class_override_property(obj_class, PROP_CHAT_Y, "chat-y");
    //TODO Move these into GtPlayer
    g_object_class_install_property(obj_class, PROP_CHAT_WIDTH, props[PROP_CHAT_WIDTH]);
    g_object_class_install_property(obj_class, PROP_CHAT_HEIGHT, props[PROP_CHAT_HEIGHT]);
}

static void
gt_player_mpv_init(GtPlayerMpv* self)
{
    GtPlayerMpvPrivate* priv = gt_player_mpv_get_instance_private(self);

    priv->mpv = mpv_create();
    priv->opengl_area = gtk_gl_area_new();
    priv->opengl_ready = FALSE;
    priv->chat_view = GTK_WIDGET(gt_twitch_chat_view_new());

    check_mpv_error(mpv_set_option_string(priv->mpv, "vo", "opengl-cb"));

    check_mpv_error(mpv_initialize(priv->mpv));

    priv->mpv_opengl = mpv_get_sub_api(priv->mpv, MPV_SUB_API_OPENGL_CB);

    mpv_opengl_cb_set_update_callback(priv->mpv_opengl, (mpv_opengl_cb_update_fn) opengl_cb, self);

    g_signal_connect(priv->opengl_area, "render", G_CALLBACK(render_cb), self);

    gtk_container_add(GTK_CONTAINER(self), priv->opengl_area);
    gtk_widget_show_all(GTK_WIDGET(self));
}
