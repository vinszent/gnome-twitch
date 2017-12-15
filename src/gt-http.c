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

#include "gt-http.h"

#define TAG "GtHTTP"
#include "gnome-twitch/gt-log.h"

G_DEFINE_INTERFACE(GtHTTP, gt_http, G_TYPE_OBJECT);

static void
gt_http_default_init(GtHTTPInterface* iface)
{
    g_autofree gchar* default_cache_directory = g_build_filename(g_get_user_cache_dir(),
        "gnome-twitch", "http-cache", NULL);

    g_object_interface_install_property(iface, g_param_spec_uint("max-inflight-per-category",
            "Max inflight per category", "Maximum allowed inflight messages per category",
            0, G_MAXUINT, 4, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_interface_install_property(iface, g_param_spec_string("cache-directory",
            "Cache directory", "Directory where cached files should be placed",
            default_cache_directory, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

void
gt_http_get(GtHTTP* http, const gchar* uri, gchar** headers,
        GCancellable* cancel, GtHTTPCallback cb, gpointer udata, gint flags)
{
    RETURN_IF_FAIL(GT_IS_HTTP(http));
    RETURN_IF_FAIL(GT_HTTP_GET_IFACE(http)->get != NULL);

    return GT_HTTP_GET_IFACE(http)->get(http, uri, headers, cancel, cb, udata, flags);
}

void
gt_http_get_with_category(GtHTTP* http, const gchar* uri, const gchar* category, gchar** headers,
    GCancellable* cancel, GtHTTPCallback cb, gpointer udata, gint flags)
{
    RETURN_IF_FAIL(GT_IS_HTTP(http));
    RETURN_IF_FAIL(GT_HTTP_GET_IFACE(http)->get != NULL);

    return GT_HTTP_GET_IFACE(http)->get_with_category(http, uri, category, headers, cancel, cb, udata, flags);
}
