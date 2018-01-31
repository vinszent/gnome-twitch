/*
 *  This file is part of GNOME Twitch - 'Enjoy Twitch on your GNU/Linux desktop'
 *  Copyright © 2017 Vincent Szolnoky <vinszent@vinszent.com>
 *
 *  GNOME Twitch is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GNOME Twitch is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNOME Twitch. If not, see <http://www.gnu.org/licenses/>.
 */

#include "gt-player-backend-mpv-opengl.h"
#include "gnome-twitch/gt-player-backend.h"
#include <mpv/client.h>
#include <mpv/opengl_cb.h>
#include <epoxy/gl.h>
#include <gdk/gdk.h>
#include <locale.h>

#define TAG "GtPlayerBackendMpvOpenGL"
#include "gnome-twitch/gt-log.h"

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
    gdouble buffer_fill;
    gboolean seekable;
    gint64 duration;
    gint64 position;
    GtPlayerBackendState state;

    gint mpv_event_cb_id;
} GtPlayerBackendMpvOpenGLPrivate;

static void gt_player_backend_iface_init(GtPlayerBackendInterface* iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(GtPlayerBackendMpvOpenGL, gt_player_backend_mpv_opengl,
    PEAS_TYPE_EXTENSION_BASE, 0,
    G_IMPLEMENT_INTERFACE_DYNAMIC(GT_TYPE_PLAYER_BACKEND, gt_player_backend_iface_init)
    G_ADD_PRIVATE_DYNAMIC(GtPlayerBackendMpvOpenGL))

enum
{
    PROP_0,
    PROP_VOLUME,
    PROP_BUFFER_FILL,
    PROP_DURATION,
    PROP_POSITION,
    PROP_SEEKABLE,
    PROP_STATE,
    NUM_PROPS,
};

static GParamSpec* props[NUM_PROPS];

/* TODO: Get rid of this and move utils stuff into a shared place */
static void
weak_ref_notify_cb(gpointer udata, GObject* obj) /* NOTE: This object is no longer valid, can only use the address */
{
    RETURN_IF_FAIL(udata != NULL);

    g_weak_ref_set(udata, NULL);
}

GWeakRef*
utils_weak_ref_new(gpointer obj)
{
    GWeakRef* ref = g_malloc(sizeof(GWeakRef));

    g_weak_ref_init(ref, obj);
    g_object_weak_ref(obj, weak_ref_notify_cb, ref);

    return ref;
}

void
utils_weak_ref_free(GWeakRef* ref)
{
    g_autoptr(GObject) obj = g_weak_ref_get(ref);

    if (obj)
        g_object_weak_unref(obj, weak_ref_notify_cb, ref);

    g_weak_ref_clear(ref);
    g_free(ref);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GWeakRef, utils_weak_ref_free);

static inline void
check_mpv_error(int status)
{
    if (status < 0)
        ERRORF("Mpv error %s\n", mpv_error_string(status));
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GParamSpec, g_param_spec_unref);

/* NOTE: This is so that we notify properies on the GLib main thread,
  * otherwise bad things can happen */
static gboolean
notify_prop_cb(gpointer udata)
{
    RETURN_VAL_IF_FAIL(G_IS_PARAM_SPEC(udata), G_SOURCE_REMOVE);

    GParamSpec* pspec = udata; /* NOTE: Will be destroyed automatically by idle_add_full */
    g_autoptr(GWeakRef) ref = g_param_spec_steal_qdata(pspec, g_quark_from_string("_gt_self"));
    g_autoptr(GtPlayerBackendMpvOpenGL) self = g_weak_ref_get(ref);

    if (!self) {TRACE("Unreffed while wating"); return G_SOURCE_REMOVE;}

    g_object_notify_by_pspec(G_OBJECT(self), pspec);

    return G_SOURCE_REMOVE;
}

#define NOTIFY_PROP(self, pprop)                                         \
    G_STMT_START                                                        \
    {                                                                   \
        g_param_spec_set_qdata_full(pprop, g_quark_from_string("_gt_self"), \
            utils_weak_ref_new(self), (GDestroyNotify) utils_weak_ref_free); \
        g_idle_add_full(G_PRIORITY_DEFAULT, notify_prop_cb,             \
            g_param_spec_ref(pprop), (GDestroyNotify) g_param_spec_unref); \
    } G_STMT_END

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
                    NOTIFY_PROP(self, props[PROP_VOLUME]);

                }
                else if (g_strcmp0(prop->name, "cache-buffering-state") == 0)
                {
                    if (prop->data) priv->buffer_fill = *((gint64*) prop->data) / 100.0;
                    NOTIFY_PROP(self, props[PROP_BUFFER_FILL]);
                }
                else if (g_strcmp0(prop->name, "time-pos") == 0)
                {
                    if (prop->data) priv->position = *((gint64*) prop->data);
                    NOTIFY_PROP(self, props[PROP_POSITION]);
                }
                else if (g_strcmp0(prop->name, "duration") == 0)
                {
                    /* NOTE: The seekable prop isn't very good because
                     * it still returns true for a live HLS stream */
                    if (prop->data)
                    {
                        priv->duration = *((gint64*) prop->data);
                        priv->seekable = priv->duration > 0;
                    }
                    NOTIFY_PROP(self, props[PROP_DURATION]);
                    NOTIFY_PROP(self, props[PROP_SEEKABLE]);
                }
                else if (g_strcmp0(prop->name, "pause") == 0)
                {
                }
                else if (g_strcmp0(prop->name, "unpause") == 0)
                {
                }
                else if (g_strcmp0(prop->name, "core-idle") == 0)
                {
                }
                else if (g_strcmp0(prop->name, "playback-abort") == 0)
                {
                    if (prop->data)
                    {
                        priv->state = *((gboolean*) prop->data) ?
                            GT_PLAYER_BACKEND_STATE_STOPPED : GT_PLAYER_BACKEND_STATE_PLAYING;
                    }
                    NOTIFY_PROP(self, props[PROP_STATE]);
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

    priv->mpv_event_cb_id = 0;

    return G_SOURCE_REMOVE;
}

static void
mpv_wakeup_cb(gpointer udata)
{
    GtPlayerBackendMpvOpenGL* self = GT_PLAYER_BACKEND_MPV_OPENGL(udata);
    GtPlayerBackendMpvOpenGLPrivate* priv = gt_player_backend_mpv_opengl_get_instance_private(self);

    priv->mpv_event_cb_id = g_idle_add_full(G_PRIORITY_DEFAULT, mpv_event_cb, g_object_ref(self), g_object_unref);
}

static void
play(GtPlayerBackend* backend)
{
    RETURN_IF_FAIL(GT_IS_PLAYER_BACKEND_MPV_OPENGL(backend));

    GtPlayerBackendMpvOpenGL* self = GT_PLAYER_BACKEND_MPV_OPENGL(backend);
    GtPlayerBackendMpvOpenGLPrivate* priv = gt_player_backend_mpv_opengl_get_instance_private(self);

    if (priv->uri)
    {
        const gchar* mpv_cmd[] = {"loadfile", priv->uri, "replace", NULL};

        check_mpv_error(mpv_command(priv->mpv, mpv_cmd));

        priv->state = GT_PLAYER_BACKEND_STATE_LOADING;
        g_object_notify_by_pspec(G_OBJECT(self), props[PROP_STATE]);
    }
}

static void
stop(GtPlayerBackend* backend)
{
    RETURN_IF_FAIL(GT_IS_PLAYER_BACKEND_MPV_OPENGL(backend));

    GtPlayerBackendMpvOpenGL* self = GT_PLAYER_BACKEND_MPV_OPENGL(backend);
    GtPlayerBackendMpvOpenGLPrivate* priv = gt_player_backend_mpv_opengl_get_instance_private(self);

    const gchar* mpv_cmd[] = {"stop", NULL};

    check_mpv_error(mpv_command(priv->mpv, mpv_cmd));
}

/* NOTE: This is so dumb but unistd.h already defines pause() */
static void
_pause(GtPlayerBackend* backend)
{
    /* TODO: Implement */
}

static void
set_volume(GtPlayerBackendMpvOpenGL* self, gdouble volume)
{
    GtPlayerBackendMpvOpenGLPrivate* priv = gt_player_backend_mpv_opengl_get_instance_private(self);

    gdouble vol = volume*100.0;

    mpv_set_property(priv->mpv, "volume", MPV_FORMAT_DOUBLE, &vol);
}

static void
set_uri(GtPlayerBackend* backend, const gchar* uri)
{
    RETURN_IF_FAIL(GT_IS_PLAYER_BACKEND_MPV_OPENGL(backend));

    GtPlayerBackendMpvOpenGL* self = GT_PLAYER_BACKEND_MPV_OPENGL(backend);
    GtPlayerBackendMpvOpenGLPrivate* priv = gt_player_backend_mpv_opengl_get_instance_private(self);

    g_free(priv->uri);
    priv->uri = g_strdup(uri);
}

static void
set_position(GtPlayerBackend* backend, gint64 position)
{
    RETURN_IF_FAIL(GT_IS_PLAYER_BACKEND_MPV_OPENGL(backend));

    GtPlayerBackendMpvOpenGL* self = GT_PLAYER_BACKEND_MPV_OPENGL(backend);
    GtPlayerBackendMpvOpenGLPrivate* priv = gt_player_backend_mpv_opengl_get_instance_private(self);

    g_autofree gchar* seconds = g_strdup_printf("%ld", position);

    const gchar* mpv_cmd[] = {"seek", seconds, "absolute", NULL};

    check_mpv_error(mpv_command_async(priv->mpv, 0, mpv_cmd));
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
        case PROP_BUFFER_FILL:
            g_value_set_double(val, priv->buffer_fill);
            break;
        case PROP_DURATION:
            g_value_set_int64(val, priv->duration);
            break;
        case PROP_POSITION:
            g_value_set_int64(val, priv->position);
            break;
        case PROP_SEEKABLE:
            g_value_set_boolean(val, priv->seekable);
            break;
        case PROP_STATE:
            g_value_set_enum(val, priv->state);
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
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
finalise(GObject* obj)
{
    GtPlayerBackendMpvOpenGL* self = GT_PLAYER_BACKEND_MPV_OPENGL(obj);
    GtPlayerBackendMpvOpenGLPrivate* priv = gt_player_backend_mpv_opengl_get_instance_private(self);

    MESSAGE("Finalise");

    if (priv->mpv_event_cb_id > 0) g_source_remove(priv->mpv_event_cb_id);

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
    iface->play = play;
    iface->stop = stop;
    iface->pause = _pause;
    iface->set_uri = set_uri;
    iface->set_position = set_position;

}

static void
gt_player_backend_mpv_opengl_class_init(GtPlayerBackendMpvOpenGLClass* klass)
{
    GObjectClass* obj_class = G_OBJECT_CLASS(klass);

    obj_class->finalize = finalise;
    obj_class->get_property = get_property;
    obj_class->set_property = set_property;

    props[PROP_VOLUME] = g_param_spec_double("volume", "Volume", "Volume of player",
        0.0, 1.0, 0.3, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_CONSTRUCT);

    props[PROP_BUFFER_FILL] = g_param_spec_double("buffer-fill", "Buffer Fill", "Current buffer fill",
        0, 1.0, 0, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    props[PROP_DURATION] = g_param_spec_int64("duration", "Duration", "Current duration",
        0, G_MAXINT64, 0, G_PARAM_READABLE);

    props[PROP_POSITION] = g_param_spec_int64("position", "Position", "Current position",
        0, G_MAXINT64, 0, G_PARAM_READWRITE);

    props[PROP_SEEKABLE] = g_param_spec_boolean("seekable", "Seekable", "Whether seekable",
        FALSE, G_PARAM_READABLE);

    props[PROP_STATE] = g_param_spec_enum("state", "State", "Current state",
        GT_TYPE_PLAYER_BACKEND_STATE, GT_PLAYER_BACKEND_STATE_STOPPED, G_PARAM_READABLE);

    g_object_class_override_property(obj_class, PROP_VOLUME, "volume");
    g_object_class_override_property(obj_class, PROP_BUFFER_FILL, "buffer-fill");
    g_object_class_override_property(obj_class, PROP_DURATION, "duration");
    g_object_class_override_property(obj_class, PROP_POSITION, "position");
    g_object_class_override_property(obj_class, PROP_SEEKABLE, "seekable");
    g_object_class_override_property(obj_class, PROP_STATE, "state");
}

static void
gt_player_backend_mpv_opengl_init(GtPlayerBackendMpvOpenGL* self)
{
    GtPlayerBackendMpvOpenGLPrivate* priv = gt_player_backend_mpv_opengl_get_instance_private(self);

    MESSAGE("Init");

    setlocale(LC_NUMERIC, "C");

    priv->uri = NULL;
    priv->state = GT_PLAYER_BACKEND_STATE_STOPPED;
    priv->buffer_fill = 0.0;

    priv->widget = gtk_gl_area_new();
    priv->mpv = mpv_create();

    g_object_set(priv->widget, "expand", TRUE, NULL);

    gtk_widget_add_events(priv->widget, GDK_BUTTON_PRESS_MASK);

    check_mpv_error(mpv_set_option_string(priv->mpv, "audio-client-name", "GNOME Twitch"));
    check_mpv_error(mpv_set_option_string(priv->mpv, "title", ""));
    check_mpv_error(mpv_set_option_string(priv->mpv, "vo", "opengl-cb"));
    check_mpv_error(mpv_set_option_string(priv->mpv, "softvol", "yes"));
    check_mpv_error(mpv_set_option_string(priv->mpv, "softvol-max", "100"));
    check_mpv_error(mpv_observe_property(priv->mpv, 0, "volume", MPV_FORMAT_DOUBLE));
    check_mpv_error(mpv_observe_property(priv->mpv, 0, "cache-buffering-state", MPV_FORMAT_INT64));
    check_mpv_error(mpv_observe_property(priv->mpv, 0, "time-pos", MPV_FORMAT_INT64));
    check_mpv_error(mpv_observe_property(priv->mpv, 0, "duration", MPV_FORMAT_INT64));
    check_mpv_error(mpv_observe_property(priv->mpv, 0, "pause", MPV_FORMAT_FLAG));
    check_mpv_error(mpv_observe_property(priv->mpv, 0, "unpause", MPV_FORMAT_FLAG));
    check_mpv_error(mpv_observe_property(priv->mpv, 0, "core-idle", MPV_FORMAT_FLAG));
    check_mpv_error(mpv_observe_property(priv->mpv, 0, "playback-abort", MPV_FORMAT_FLAG));

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
        GT_TYPE_PLAYER_BACKEND, GT_TYPE_PLAYER_BACKEND_MPV_OPENGL);
}
