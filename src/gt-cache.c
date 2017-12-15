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

#include "gt-cache.h"

#define TAG "GtCache"
#include "gnome-twitch/gt-log.h"

G_DEFINE_INTERFACE(GtCache, gt_cache, G_TYPE_OBJECT)

static void
gt_cache_default_init(GtCacheInterface* iface)
{
    g_autofree gchar* default_cache_directory = g_build_filename(g_get_user_cache_dir(),
        "gnome-twitch", "http-cache", NULL);

    g_object_interface_install_property(iface, g_param_spec_string("cache-directory",
            "Cache directory", "Directory where cached files should be placed",
            default_cache_directory, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

void
gt_cache_save_data(GtCache* cache, const gchar* key, gconstpointer data, gsize length, GDateTime* last_updated, GDateTime* expiry, const gchar* etag)
{
    RETURN_IF_FAIL(GT_IS_CACHE(cache));
    RETURN_IF_FAIL(GT_CACHE_GET_IFACE(cache)->save_data != NULL);

    return GT_CACHE_GET_IFACE(cache)->save_data(cache, key, data, length, last_updated, expiry, etag);
}

GInputStream*
gt_cache_get_data_stream(GtCache* cache, const gchar* key, GError** error)
{
    RETURN_VAL_IF_FAIL(GT_IS_CACHE(cache), NULL);
    RETURN_VAL_IF_FAIL(GT_CACHE_GET_IFACE(cache)->get_data_stream != NULL, NULL);

    return GT_CACHE_GET_IFACE(cache)->get_data_stream(cache, key, error);
}

gboolean
gt_cache_is_data_stale(GtCache* cache, const gchar* key, GDateTime* last_updated, const gchar* etag)
{
    RETURN_VAL_IF_FAIL(GT_IS_CACHE(cache), TRUE);
    RETURN_VAL_IF_FAIL(GT_CACHE_GET_IFACE(cache)->is_data_stale != NULL, TRUE);

    return GT_CACHE_GET_IFACE(cache)->is_data_stale(cache, key, last_updated, etag);
}
