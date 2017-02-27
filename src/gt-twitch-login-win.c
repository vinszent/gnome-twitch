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

#include "gt-twitch-login-win.h"

typdef struct
{
    void* tmp;
} GtTwitchLoginWinPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GtTwitchLoginWin, gt_twitch_login_win, GTK_TYPE_DIALOG)

enum
{
    PROP_0,
    NUM_PROPS
};

static GParamSpec* props[NUM_PROPS];

GtTwitchLoginWin*
gt_twitch_login_win_new()
{
    return g_object_new(GT_TWITCH_LOGIN_WIN,
                        NULL);
}

static void
finalise(GObject* obj)
{
    GtTwitchLoginWin* self = GT_TWITCH_LOGIN_WIN(obj);
    GtTwitchLoginWinPrivate* priv = gt_twitch_login_win_get_instance_private(self);

    G_OBJECT_CLASS(gt_twitch_login_win_parent_class)->finalize(obj);
}

static void
get_property(GObject* obj,
             guint prop,
             GValue* val,
             GParamSpec* pspec)
{
    GtTwitchLoginWin* self = GT_TWITCH_LOGIN_WIN(obj);
    GtTwitchLoginWinPrivate* priv = gt_twitch_login_win_get_instance_private(self);

    switch (prop)
    {
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
    GtTwitchLoginWin* self = GT_TWITCH_LOGIN_WIN(obj);
    GtTwitchLoginWinPrivate* priv = gt_twitch_login_win_get_instance_private(self);

    switch (prop)
    {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
    }
}

static void
gt_twitch_login_win_class_init(GtTwitchLoginWinClass* klass)
{
    GObjectClass* obj_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);

    obj_class->finalize = finalise;
    obj_class->get_property = get_property;
    obj_class->set_property = set_property;

    gtk_widget_class_set_template_from_resource(widget_class, "/com/vinszent/GnomeTwitch/ui/gt-twitch-login-dlg.ui");
}

static void
gt_twitch_login_win_init(GtTwitchLoginWin* self)
{
    gtk_widget_init_template(GTK_WIDGET(self));
}
