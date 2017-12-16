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

#ifndef GT_HTTP_H
#define GT_HTTP_H

#include "config.h"
#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GT_TYPE_HTTP gt_http_get_type()

G_DECLARE_INTERFACE(GtHTTP, gt_http, GT, HTTP, GObject);

typedef void (*GtHTTPCallback) (GtHTTP* http, gpointer ret, GError* error, gpointer user_data);

typedef enum
{
    GT_HTTP_FLAG_RETURN_STREAM  = 1,
    GT_HTTP_FLAG_RETURN_DATA    = 1 << 2,
    GT_HTTP_FLAG_CACHE_RESPONSE = 1 << 3,
} GtHTTPFlag;

#define GT_HTTP_ERROR g_quark_from_static_string("gt-http-error-quark")

typedef enum
{
    GT_HTTP_ERROR_UNSUCCESSFUL_RESPONSE,
    GT_HTTP_ERROR_UNKNOWN,
} GtHTTPError;

static gchar* GT_HTTP_NO_HEADERS[] = {NULL};

static gchar* GT_HTTP_TWITCH_HLS_HEADERS[] =
{
    "Client-ID", CLIENT_ID,
    "Accept", "application/vnd.apple.mpegurl",
    NULL,
};

static gchar* DEFAULT_TWITCH_HEADERS[]  =
{
    "Client-ID", CLIENT_ID,
    "Accept", "application/vnd.twitchtv.v5+json",
    "Accept", "image/*",
    NULL,
};

struct _GtHTTPInterface
{
    GTypeInterface parent_interface;

    void (*get) (GtHTTP* http, const gchar* uri, gchar** headers,
        GCancellable* cancel, GtHTTPCallback cb, gpointer udata, gint flags);
    void (*get_with_category) (GtHTTP* http, const gchar* uri, const gchar* category, gchar** headers,
        GCancellable* cancel, GtHTTPCallback cb, gpointer udata, gint flags);
};

/* TODO: Add docs */
void gt_http_get(GtHTTP* http, const gchar* uri, gchar** headers,
        GCancellable* cancel, GtHTTPCallback cb, gpointer udata, gint flags);
void gt_http_get_with_category(GtHTTP* http, const gchar* uri, const gchar* category, gchar** headers,
    GCancellable* cancel, GtHTTPCallback cb, gpointer udata, gint flags);

G_END_DECLS

#endif
