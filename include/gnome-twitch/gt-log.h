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
#define LOG(lvl, msg) g_log(NULL, (GLogLevelFlags) lvl, "{%s} %s", TAG, msg)
#define LOGF(lvl, fmt, ...) g_log(NULL, (GLogLevelFlags) lvl, "{%s} " fmt, TAG, __VA_ARGS__)
#define FATAL(msg) LOG(GT_LOG_LEVEL_WARNING, msg)
#define FATALF(fmt, ...) LOGF(GT_LOG_LEVEL_WARNING, fmt, __VA_ARGS__)
#define ERROR(msg) LOG(GT_LOG_LEVEL_WARNING, msg)
#define ERRORF(fmt, ...) LOGF(GT_LOG_LEVEL_WARNING, fmt, __VA_ARGS__)
#define CRITICAL(msg) LOG(GT_LOG_LEVEL_WARNING, msg)
#define CRITICALF(fmt, ...) LOGF(GT_LOG_LEVEL_WARNING, fmt, __VA_ARGS__)
#define WARNING(msg) LOG(GT_LOG_LEVEL_WARNING, msg)
#define WARNINGF(fmt, ...) LOGF(GT_LOG_LEVEL_WARNING, fmt, __VA_ARGS__)
#define MESSAGE(msg) LOG(GT_LOG_LEVEL_MESSAGE, msg)
#define MESSAGEF(fmt, ...) LOGF(GT_LOG_LEVEL_MESSAGE, fmt, __VA_ARGS__)
#define INFO(msg) LOG(GT_LOG_LEVEL_INFO, msg)
#define INFOF(fmt, ...) LOGF(GT_LOG_LEVEL_INFO, fmt, __VA_ARGS__)
#define DEBUG(msg) LOG(GT_LOG_LEVEL_DEBUG, msg)
#define DEBUGF(fmt, ...) LOGF(GT_LOG_LEVEL_DEBUG, fmt, __VA_ARGS__)
#define TRACE(msg) LOG(GT_LOG_LEVEL_TRACE, msg)
#define TRACEF(fmt, ...) LOGF(GT_LOG_LEVEL_TRACE, fmt, __VA_ARGS__)
#endif

#endif
