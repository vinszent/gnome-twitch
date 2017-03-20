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

#ifndef _GT_LOG_H
#define _GT_LOG_H

#include <glib.h>

typedef enum
{
    // Report most mundane info like http responses, irc responses, etc.
    // Generally reporting raw data.
    GT_LOG_LEVEL_TRACE    = 1 << (G_LOG_LEVEL_USER_SHIFT + 6),
    // 'Spammy' info like cache hits/misses, interesting object references, http requests
    GT_LOG_LEVEL_DEBUG    = 1 << (G_LOG_LEVEL_USER_SHIFT + 5),
    // Less 'spammy' info but not useful to user.
    // Init of stuff, finalisation of stuff, etc.
    GT_LOG_LEVEL_INFO     = 1 << (G_LOG_LEVEL_USER_SHIFT + 4),
    // Info that the end user might find useful. Things that tend to only happen
    // a handful of times. Setting signing in, favouriting channels, etc.
    GT_LOG_LEVEL_MESSAGE  = 1 << (G_LOG_LEVEL_USER_SHIFT + 3),
    // Warning mainly to alert devs of potential errors but GT can still
    // function like normal. Failed http requests, opening of settings files, etc.
    // Can show error to user.
    GT_LOG_LEVEL_WARNING  = 1 << (G_LOG_LEVEL_USER_SHIFT + 2),
    // Like warning but more critical. Should definitely show error to user.
    // GT might not function like normal, but will still run.
    GT_LOG_LEVEL_CRITICAL = 1 << (G_LOG_LEVEL_USER_SHIFT + 1),
    // Boom, crash.
    GT_LOG_LEVEL_ERROR    = 1 << G_LOG_LEVEL_USER_SHIFT,
} GtLogLevelFlags;

#ifndef TAG
#error Tag not defined
#else
#define LOG(lvl, msg, ...) g_log(NULL, (GLogLevelFlags) lvl, "{%s:%d} %s", TAG, __LINE__, msg, ##__VA_ARGS__)
#define LOGF(lvl, fmt, ...) g_log(NULL, (GLogLevelFlags) lvl, "{%s:%d} " fmt, TAG, __LINE__, ##__VA_ARGS__)
#define FATAL(msg, ...) LOGF(GT_LOG_LEVEL_WARNING, msg, ##__VA_ARGS__)
#define FATALF(fmt, ...) LOGF(GT_LOG_LEVEL_WARNING, fmt, __VA_ARGS__)
#define ERROR(msg, ...) LOGF(GT_LOG_LEVEL_ERROR, msg, ##__VA_ARGS__)
#define ERRORF(fmt, ...) LOGF(GT_LOG_LEVEL_ERROR, fmt, __VA_ARGS__)
#define CRITICAL(msg, ...) LOGF(GT_LOG_LEVEL_CRITICAL, msg, ##__VA_ARGS__)
#define CRITICALF(fmt, ...) LOGF(GT_LOG_LEVEL_CRITICAL, fmt, __VA_ARGS__)
#define WARNING(msg, ...) LOGF(GT_LOG_LEVEL_WARNING, msg, ##__VA_ARGS__)
#define WARNINGF(fmt, ...) LOGF(GT_LOG_LEVEL_WARNING, fmt, __VA_ARGS__)
#define MESSAGE(msg, ...) LOGF(GT_LOG_LEVEL_MESSAGE, msg, ##__VA_ARGS__)
#define MESSAGEF(fmt, ...) LOGF(GT_LOG_LEVEL_MESSAGE, fmt, __VA_ARGS__)
#define INFO(msg, ...) LOGF(GT_LOG_LEVEL_INFO, msg, ##__VA_ARGS__)
#define INFOF(fmt, ...) LOGF(GT_LOG_LEVEL_INFO, fmt, __VA_ARGS__)
#define DEBUG(msg, ...) LOGF(GT_LOG_LEVEL_DEBUG, msg, ##__VA_ARGS__)
#define DEBUGF(fmt, ...) LOGF(GT_LOG_LEVEL_DEBUG, fmt, __VA_ARGS__)
#define TRACE(msg, ...) LOGF(GT_LOG_LEVEL_TRACE, msg, ##__VA_ARGS__)
#define TRACEF(fmt, ...) LOGF(GT_LOG_LEVEL_TRACE, fmt, __VA_ARGS__)


#define RETURN_IF_FAIL(expr)                                \
    if (!(expr))                                            \
    {                                                       \
        CRITICAL("Expression '%s' should be TRUE", #expr);  \
        return;                                             \
    }
#define RETURN_VAL_IF_FAIL(expr, val)                       \
    if (!(expr))                                            \
    {                                                       \
        CRITICAL("Expression '%s' should be TRUE", #expr);  \
        return val;                                         \
    }
#define RETURN_IF_REACHED()                             \
    CRITICAL("This expression should not be reached");  \
    return;
#define RETURN_VAL_IF_REACHED(val)                      \
    CRITICAL("This expression should not be reached");  \
    return val;
#endif

#endif
