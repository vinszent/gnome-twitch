#include "gt-player-backend-mpv-opengl.h"
#include "gnome-twitch/gt-player-backend.h"
#include <mpv/client.h>
#include <mpv/opengl_cb.h>
#include <epoxy/gl.h>
#include <gdk/gdk.h>
#include <locale.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#include <epoxy/glx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#include <epoxy/egl.h>
#endif
#ifdef GDK_WINDOWING_WIN32
#include <gdk/gdkwin32.h>
#include <epoxy/wgl.h>
#endif

typedef struct
{
    mpv_handle* mpv;
    mpv_opengl_cb_context* mpv_opengl;

    GtkWidget* widget;

    gchar* uri;

    gdouble volume;
    gboolean playing;
    gdouble buffer_fill;
} GtPlayerBackendMpvOpenGLPrivate;

static void gt_player_backend_iface_init(GtPlayerBackendInterface* iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(GtPlayerBackendMpvOpenGL, gt_player_backend_mpv_opengl, PEAS_TYPE_EXTENSION_BASE, 0,
                               G_IMPLEMENT_INTERFACE_DYNAMIC(GT_TYPE_PLAYER_BACKEND, gt_player_backend_iface_init)
                               G_ADD_PRIVATE_DYNAMIC(GtPlayerBackendMpvOpenGL))

enum
{
    PROP_0,
    PROP_VOLUME,
    PROP_PLAYING,
    PROP_URI,
    PROP_BUFFER_FILL,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

static inline void
check_mpv_error(int status)
{
    if (status < 0)
        g_error("{GtPlayerMpv} Mpv error %s\n", mpv_error_string(status));
}

static gboolean
mpv_event_cb(gpointer udata)
{
    GtPlayerBackendMpvOpenGL* self = GT_PLAYER_BACKEND_MPV_OPENGL(udata);
    GtPlayerBackendMpvOpenGLPrivate* priv = gt_player_backend_mpv_opengl_get_instance_private(self);
    gboolean done = FALSE;

    while (!done)
    {
        mpv_event* evt = mpv_wait_event(priv->mpv, 0);

        switch (evt->event_id)
        {
            case MPV_EVENT_PROPERTY_CHANGE:
            {
                mpv_event_property* prop = evt->data;

                if (g_strcmp0(prop->name, "volume") == 0)
                {
                    if (prop->data) priv->volume = *((double*) prop->data) / 100.0;
                    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_VOLUME]);
                }
                else if (g_strcmp0(prop->name, "cache-buffering-state") == 0)
                {
                    if (prop->data) priv->buffer_fill = *((gint64*) prop->data) / 100.0;
                    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_BUFFER_FILL]);
                }

                break;
            }
            case MPV_EVENT_NONE:
                done = TRUE;
                break;
            default:
                break;
        }
    }

    return G_SOURCE_REMOVE;
}

static void
mpv_wakeup_cb(gpointer udata)
{
    g_idle_add(mpv_event_cb, udata);
}

static void
play(GtPlayerBackendMpvOpenGL* self)
{
    GtPlayerBackendMpvOpenGLPrivate* priv = gt_player_backend_mpv_opengl_get_instance_private(self);

    if (priv->uri)
    {
        const gchar* mpv_cmd[] = {"loadfile", priv->uri, "replace", NULL};

        check_mpv_error(mpv_command(priv->mpv, mpv_cmd));
    }
}

static void
stop(GtPlayerBackendMpvOpenGL* self)
{
    GtPlayerBackendMpvOpenGLPrivate* priv = gt_player_backend_mpv_opengl_get_instance_private(self);

    const gchar* mpv_cmd[] = {"stop", NULL};

    check_mpv_error(mpv_command(priv->mpv, mpv_cmd));
}

static void
set_volume(GtPlayerBackendMpvOpenGL* self, gdouble volume)
{
    GtPlayerBackendMpvOpenGLPrivate* priv = gt_player_backend_mpv_opengl_get_instance_private(self);

    gdouble vol = volume*100.0;

    mpv_set_property(priv->mpv, "volume", MPV_FORMAT_DOUBLE, &vol);
}

static GtkWidget*
get_widget(GtPlayerBackend* backend)
{
    GtPlayerBackendMpvOpenGL* self = GT_PLAYER_BACKEND_MPV_OPENGL(backend);
    GtPlayerBackendMpvOpenGLPrivate* priv = gt_player_backend_mpv_opengl_get_instance_private(self);

    return priv->widget;
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
    #ifdef GDK_WINDOWING_WIN32
    if (GDK_IS_WIN32_DISPLAY(display))
        return wglGetProcAddress(name);
    #endif

    g_assert_not_reached();
}

static gboolean
queue_render_cb(gpointer udata)
{
    if (udata) //Might have been destroyed
        gtk_gl_area_queue_render(GTK_GL_AREA(udata));

    return FALSE;
}

static void
opengl_cb(gpointer udata)
{
    GtPlayerBackendMpvOpenGL* self = GT_PLAYER_BACKEND_MPV_OPENGL(udata);
    GtPlayerBackendMpvOpenGLPrivate* priv = gt_player_backend_mpv_opengl_get_instance_private(self);

    //So we don't hog the main thread
    g_idle_add_full(G_PRIORITY_HIGH,
                    (GSourceFunc) queue_render_cb,
                    priv->widget,
                    NULL);
}

static gboolean
render_cb(GtkGLArea* area,
          GdkGLContext* ctxt,
          gpointer udata)
{
    GtPlayerBackendMpvOpenGL* self = GT_PLAYER_BACKEND_MPV_OPENGL(udata);
    GtPlayerBackendMpvOpenGLPrivate* priv = gt_player_backend_mpv_opengl_get_instance_private(self);

    int width;
    int height;
    int fbo;

    width = gtk_widget_get_allocated_width(GTK_WIDGET(area));
    height = (-1)*gtk_widget_get_allocated_height(GTK_WIDGET(area));
    fbo = -1;

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
    mpv_opengl_cb_draw(priv->mpv_opengl, fbo, width, height);

    return GDK_EVENT_STOP;
}

static void
realise_oneshot_cb(GtkWidget* widget,
                   gpointer udata)
{
    GtPlayerBackendMpvOpenGL* self = GT_PLAYER_BACKEND_MPV_OPENGL(udata);
    GtPlayerBackendMpvOpenGLPrivate* priv = gt_player_backend_mpv_opengl_get_instance_private(self);

    gtk_gl_area_make_current(GTK_GL_AREA(priv->widget));

    check_mpv_error(mpv_opengl_cb_init_gl(priv->mpv_opengl, NULL,
                                          get_proc_address, NULL));

    g_signal_handlers_disconnect_by_func(widget, realise_oneshot_cb, udata);
}

static void
widget_destroy_cb(GtkWidget* widget,
                  gpointer udata)
{
    GtPlayerBackendMpvOpenGL* self = GT_PLAYER_BACKEND_MPV_OPENGL(udata);
    GtPlayerBackendMpvOpenGLPrivate* priv = gt_player_backend_mpv_opengl_get_instance_private(self);

    gtk_widget_destroyed(widget, &priv->widget);
}

static void
get_property(GObject* obj,
             guint prop,
             GValue* val,
             GParamSpec* pspec)
{
    GtPlayerBackendMpvOpenGL* self = GT_PLAYER_BACKEND_MPV_OPENGL(obj);
    GtPlayerBackendMpvOpenGLPrivate* priv = gt_player_backend_mpv_opengl_get_instance_private(self);

    switch (prop)
    {
        case PROP_VOLUME:
            g_value_set_double(val, priv->volume);
            break;
        case PROP_PLAYING:
            g_value_set_boolean(val, priv->playing);
            break;
        case PROP_URI:
            g_value_set_string(val, priv->uri);
            break;
        case PROP_BUFFER_FILL:
            g_value_set_double(val, priv->buffer_fill);
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
    GtPlayerBackendMpvOpenGL* self = GT_PLAYER_BACKEND_MPV_OPENGL(obj);
    GtPlayerBackendMpvOpenGLPrivate* priv = gt_player_backend_mpv_opengl_get_instance_private(self);

    switch (prop)
    {
        case PROP_VOLUME:
            priv->volume = g_value_get_double(val);
            set_volume(self, priv->volume);
            break;
        case PROP_PLAYING:
            priv->playing = g_value_get_boolean(val);
            priv->playing ? play(self) : stop(self);
            break;
        case PROP_URI:
            g_free(priv->uri);
            priv->uri = g_value_dup_string(val);
            break;
        case PROP_BUFFER_FILL:
            priv->buffer_fill = g_value_get_double(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
finalise(GObject* obj)
{
    GtPlayerBackendMpvOpenGL* self = GT_PLAYER_BACKEND_MPV_OPENGL(obj);
    GtPlayerBackendMpvOpenGLPrivate* priv = gt_player_backend_mpv_opengl_get_instance_private(self);

    g_message("{GtPlayerBackendMpvOpenGL} Finalise");

    mpv_set_wakeup_callback(priv->mpv, NULL, NULL);
    mpv_opengl_cb_set_update_callback(priv->mpv_opengl, NULL, NULL);
    mpv_opengl_cb_uninit_gl(priv->mpv_opengl);
    mpv_terminate_destroy(priv->mpv);

    g_clear_object(&priv->widget);

    g_free(priv->uri);

    G_OBJECT_CLASS(gt_player_backend_mpv_opengl_parent_class)->finalize(obj);
}

static void
gt_player_backend_mpv_opengl_class_finalize(GtPlayerBackendMpvOpenGLClass* klass)
{
}

static void
gt_player_backend_iface_init(GtPlayerBackendInterface* iface)
{
    iface->get_widget = get_widget;
}

static void
gt_player_backend_mpv_opengl_class_init(GtPlayerBackendMpvOpenGLClass* klass)
{
    GObjectClass* obj_class = G_OBJECT_CLASS(klass);

    obj_class->finalize = finalise;
    obj_class->get_property = get_property;
    obj_class->set_property = set_property;

    props[PROP_VOLUME] = g_param_spec_double("volume",
                                             "Volume",
                                             "Volume of player",
                                             0.0, 1.0, 0.3,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
    props[PROP_PLAYING] = g_param_spec_boolean("playing",
                                               "Playing",
                                               "Whether playing",
                                               FALSE,
                                               G_PARAM_READABLE | G_PARAM_CONSTRUCT);
    props[PROP_URI] = g_param_spec_string("uri",
                                          "Uri",
                                          "Current uri",
                                          NULL,
                                          G_PARAM_READWRITE);
    props[PROP_BUFFER_FILL] = g_param_spec_double("buffer-fill",
                                                     "Buffer Fill",
                                                     "Current buffer fill",
                                                     0, 1.0, 0,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    g_object_class_override_property(obj_class, PROP_VOLUME, "volume");
    g_object_class_override_property(obj_class, PROP_PLAYING, "playing");
    g_object_class_override_property(obj_class, PROP_URI, "uri");
    g_object_class_override_property(obj_class, PROP_BUFFER_FILL, "buffer-fill");
}

static void
gt_player_backend_mpv_opengl_init(GtPlayerBackendMpvOpenGL* self)
{
    GtPlayerBackendMpvOpenGLPrivate* priv = gt_player_backend_mpv_opengl_get_instance_private(self);

    g_message("{GtPlayerBackendMpvOpenGL} Init");

    setlocale(LC_NUMERIC, "C");

    priv->widget = gtk_gl_area_new();
    priv->mpv = mpv_create();

    g_object_set(priv->widget, "expand", TRUE, NULL);

    check_mpv_error(mpv_set_option_string(priv->mpv, "audio-client-name", "GNOME Twitch"));
    check_mpv_error(mpv_set_option_string(priv->mpv, "title", ""));
    check_mpv_error(mpv_set_option_string(priv->mpv, "vo", "opengl-cb"));
    check_mpv_error(mpv_set_option_string(priv->mpv, "softvol", "yes"));
    check_mpv_error(mpv_set_option_string(priv->mpv, "softvol-max", "100"));
    check_mpv_error(mpv_observe_property(priv->mpv, 0, "volume", MPV_FORMAT_DOUBLE));
    check_mpv_error(mpv_observe_property(priv->mpv, 0, "cache-buffering-state", MPV_FORMAT_INT64));

    check_mpv_error(mpv_initialize(priv->mpv));

    mpv_set_wakeup_callback(priv->mpv, mpv_wakeup_cb, self);

    priv->mpv_opengl = mpv_get_sub_api(priv->mpv, MPV_SUB_API_OPENGL_CB);

    mpv_opengl_cb_set_update_callback(priv->mpv_opengl, (mpv_opengl_cb_update_fn) opengl_cb, self);

    g_signal_connect(priv->widget, "destroy", G_CALLBACK(widget_destroy_cb), self);
    g_signal_connect(priv->widget, "realize", G_CALLBACK(realise_oneshot_cb), self);
    g_signal_connect(priv->widget, "render", G_CALLBACK(render_cb), self);
}

G_MODULE_EXPORT
void
peas_register_types(PeasObjectModule* module)
{
    gt_player_backend_mpv_opengl_register_type(G_TYPE_MODULE(module));

    peas_object_module_register_extension_type(module,
                                               GT_TYPE_PLAYER_BACKEND,
                                               GT_TYPE_PLAYER_BACKEND_MPV_OPENGL);
}
