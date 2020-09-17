#ifndef __OSSO_ABOOK_DEBUG_H_INCLUDED__
#define __OSSO_ABOOK_DEBUG_H_INCLUDED__

#include <libebook/libebook.h>
#include <gdk/gdk.h>

#include "osso-abook-contact.h"

G_BEGIN_DECLS

/**
 * OssoABookDebugFlags:
 * @OSSO_ABOOK_DEBUG_NONE: show no debugging messages
 * @OSSO_ABOOK_DEBUG_EDS:trace Evolution Data Server related behavior
 * @OSSO_ABOOK_DEBUG_GTK:trace GTK+ related behavior
 * @OSSO_ABOOK_DEBUG_HILDON:trace Hildon related behavior
 * @OSSO_ABOOK_DEBUG_CONTACT_ADD: trace contact additions
 * @OSSO_ABOOK_DEBUG_CONTACT_REMOVE: trace contact removals
 * @OSSO_ABOOK_DEBUG_CONTACT_UPDATE: trace contact updates
 * @OSSO_ABOOK_DEBUG_VCARD: trace vCard parsing related behavior
 * @OSSO_ABOOK_DEBUG_AVATAR: trace #OssoABookAvatar related behavior
 * @OSSO_ABOOK_DEBUG_TP: trace Telepathy related behavior
 * @OSSO_ABOOK_DEBUG_CAPS: trace #OssoABookCaps related behavior
 * @OSSO_ABOOK_DEBUG_GENERIC: trace generic behavior
 * @OSSO_ABOOK_DEBUG_LIST_STORE: trace #OssoABookListStore related behavior
 * @OSSO_ABOOK_DEBUG_STARTUP: trace startup behavior
 * @OSSO_ABOOK_DEBUG_DBUS: trace D-Bus related behavior
 * @OSSO_ABOOK_DEBUG_AGGREGATOR: trace #OssoABookAggregator related behavior
 * @OSSO_ABOOK_DEBUG_I18N: trace localization related behavior
 * @OSSO_ABOOK_DEBUG_SIM: trace sim-card related behavior
 * @OSSO_ABOOK_DEBUG_EDITOR: trace #OssoABookContactEditor related behavior
 * @OSSO_ABOOK_DEBUG_TREE_VIEW: trace #OssoABookTreeView related behavior
 * @OSSO_ABOOK_DEBUG_DISK_SPACE: assume that no disk space is available
 * @OSSO_ABOOK_DEBUG_TODO: show TODO notes
 * @OSSO_ABOOK_DEBUG_ALL: show all debugging messages
 *
 * This flags describe the various debugging domains of libosso-abook.
 */
typedef enum {
        OSSO_ABOOK_DEBUG_NONE           =  0,
        OSSO_ABOOK_DEBUG_EDS            = (1 << 0),
        OSSO_ABOOK_DEBUG_GTK            = (1 << 1),
        OSSO_ABOOK_DEBUG_HILDON         = (1 << 2),
        OSSO_ABOOK_DEBUG_CONTACT_ADD    = (1 << 3),
        OSSO_ABOOK_DEBUG_CONTACT_REMOVE = (1 << 4),
        OSSO_ABOOK_DEBUG_CONTACT_UPDATE = (1 << 5),
        OSSO_ABOOK_DEBUG_VCARD          = (1 << 6),
        OSSO_ABOOK_DEBUG_AVATAR         = (1 << 7),
        OSSO_ABOOK_DEBUG_TP             = (1 << 8),
        OSSO_ABOOK_DEBUG_CAPS           = (1 << 9),
        OSSO_ABOOK_DEBUG_GENERIC        = (1 << 10),
        OSSO_ABOOK_DEBUG_LIST_STORE     = (1 << 11),
        OSSO_ABOOK_DEBUG_STARTUP        = (1 << 12),
        OSSO_ABOOK_DEBUG_DBUS           = (1 << 13),
        OSSO_ABOOK_DEBUG_AGGREGATOR     = (1 << 14),
        OSSO_ABOOK_DEBUG_I18N           = (1 << 15),
        OSSO_ABOOK_DEBUG_SIM            = (1 << 16),
        OSSO_ABOOK_DEBUG_EDITOR         = (1 << 17),
        OSSO_ABOOK_DEBUG_TREE_VIEW      = (1 << 29),
        OSSO_ABOOK_DEBUG_DISK_SPACE     = (1 << 30),
        OSSO_ABOOK_DEBUG_TODO           = (1 << 31),
        OSSO_ABOOK_DEBUG_ALL            = ~0,
} OssoABookDebugFlags;

/* exported, but you should never use it */
extern guint _osso_abook_debug_flags;

void
osso_abook_debug_init                (void);

gboolean
osso_abook_debug_write_pixbuf        (GdkPixbuf          *pixbuf,
                                      const char         *filename,
                                      OssoABookContact   *contact);

gboolean
osso_abook_debug_write_pixbuf_prefix (GdkPixbuf          *pixbuf,
                                      const char         *filename_prefix,
                                      OssoABookContact   *contact);

#ifdef OSSO_ABOOK_DEBUG

double
_osso_abook_debug_timestamp          (void);

void
_osso_abook_log                      (const char         *domain,
                                      const char         *strloc,
                                      const char         *strfunc,
                                      const char         *strtype,
                                      OssoABookDebugFlags type,
                                      const char         *format,
                                                          ...);

void
_osso_abook_dump_vcard               (const char         *domain,
                                      const char         *strloc,
                                      const char         *strfunc,
                                      const char         *strtype,
                                      OssoABookDebugFlags type,
                                      const char         *note,
                                      EVCard             *vcard);

void
_osso_abook_dump_vcard_string        (const char         *domain,
                                      const char         *strloc,
                                      const char         *strfunc,
                                      const char         *strtype,
                                      OssoABookDebugFlags type,
                                      const char         *note,
                                      const char         *vcard);

void
_osso_abook_timer_start              (const char         *domain,
                                      const char         *strfunc,
                                      const char         *strtype,
                                      OssoABookDebugFlags type,
                                      GTimer             *timer,
                                      const char         *name);
void
_osso_abook_timer_mark               (const char         *strloc,
                                      const char         *strfunc,
                                      GTimer             *timer);

/**
 * OSSO_ABOOK_TIMER_START:
 * @timer: a #GTimer, or %NULL
 * @type: the required debugging flag, see #OssoABookDebugFlags
 * @name: the name of the timer, or %NULL
 *
 * Initializes a #GTimer object for debugging when the debugging flag
 * described by @type is set. Assigns @name for later reference,
 * or G_STRFUNC when @name is %NULL.
 */
#define OSSO_ABOOK_TIMER_START(timer, type, name)                            \
        _osso_abook_timer_start                                              \
                (G_LOG_DOMAIN, G_STRFUNC, "[" #type "]",                     \
                 OSSO_ABOOK_DEBUG_##type, (timer), (name))

/**
 * OSSO_ABOOK_TIMER_MARK:
 * @timer: a #GTimer, or %NULL
 *
 * Prints the current timestamp of @timer when the debugging flag
 * specified with OSSO_ABOOK_TIMER_START() is set.
 */
#define OSSO_ABOOK_TIMER_MARK(timer)                                         \
        _osso_abook_timer_mark (G_STRLOC, G_STRFUNC, (timer))

/**
 * OSSO_ABOOK_LOCAL_TIMER_START:
 * @type: the required debugging flag, see #OssoABookDebugFlags
 * @name: the name of the timer, or %NULL
 *
 * Initializes a local timer object for debugging when the debugging flag
 * described by @type is set. Assigns @name for later reference,
 * or G_STRFUNC when @name is %NULL.
 *
 * Requires a matching OSSO_ABOOK_LOCAL_TIMER_END() call within the same scope.
 */
#define OSSO_ABOOK_LOCAL_TIMER_START(type, name)        G_STMT_START {       \
        GTimer *_osso_abook_local_timer = (G_UNLIKELY                        \
                (_osso_abook_debug_flags & OSSO_ABOOK_DEBUG_##type) ?        \
                 g_timer_new () : NULL);                                     \
                                                                             \
        OSSO_ABOOK_TIMER_START (_osso_abook_local_timer, type, (name));

/**
 * OSSO_ABOOK_LOCAL_TIMER_END:
 *
 * Destroys a local timer created with OSSO_ABOOK_LOCAL_TIMER_START().
 */
#define OSSO_ABOOK_LOCAL_TIMER_END()                                         \
        if (G_UNLIKELY (_osso_abook_local_timer)) {                          \
                OSSO_ABOOK_TIMER_MARK (_osso_abook_local_timer);             \
                g_timer_destroy (_osso_abook_local_timer);                   \
        }                                               } G_STMT_END

/**
 * OSSO_ABOOK_NOTE:
 * @type: the required debugging flag, see #OssoABookDebugFlags
 * @format: a standard printf() format string
 * @...: the arguments to insert in the output
 *
 * Prints a debug message when the debug flags cover @type.
 */
#define OSSO_ABOOK_NOTE(type,format,...)                G_STMT_START {       \
        if (G_UNLIKELY (_osso_abook_debug_flags & OSSO_ABOOK_DEBUG_##type)) {\
                _osso_abook_log                                              \
                        (G_LOG_DOMAIN, G_STRLOC, G_STRFUNC, "[" #type "]",   \
                         OSSO_ABOOK_DEBUG_##type, (format), ## __VA_ARGS__); \
        }                                               } G_STMT_END

/**
 * OSSO_ABOOK_MARK:
 * @type: the required debugging flag, see #OssoABookDebugFlags
 *
 * Prints a debugging marker when the debug flags cover @type.
 */
#define OSSO_ABOOK_MARK(type)                           G_STMT_START {       \
        if (G_UNLIKELY (_osso_abook_debug_flags & OSSO_ABOOK_DEBUG_##type)) {\
                _osso_abook_log                                              \
                        (G_LOG_DOMAIN, G_STRLOC, G_STRFUNC, "[" #type "]",   \
                         OSSO_ABOOK_DEBUG_##type, "======================"   \
                         "==============================================="); \
        }                                               } G_STMT_END

/**
 * OSSO_ABOOK_DUMP_VCARD:
 * @type: the required debugging flag, see #OssoABookDebugFlags
 * @contact: the #EContact to dump
 * @note: description of @contact, or %NULL
 *
 * Prints the attributes of @contact when the debug flags cover @type.
 */
#define OSSO_ABOOK_DUMP_VCARD(type,contact,note)        G_STMT_START {       \
        if (G_UNLIKELY (_osso_abook_debug_flags & OSSO_ABOOK_DEBUG_##type)) {\
                _osso_abook_dump_vcard                                       \
                        (G_LOG_DOMAIN, G_STRLOC, G_STRFUNC, "[" #type "]",   \
                         OSSO_ABOOK_DEBUG_##type, (note), E_VCARD (contact));\
        }                                               } G_STMT_END

/**
 * OSSO_ABOOK_DUMP_VCARD_STRING:
 * @type: the required debugging flag, see #OssoABookDebugFlags
 * @vcs: the vcard string to dump
 * @note: description of @vcs, or %NULL
 *
 * Prints the attributes of @vcs when the debug flags cover @type.
 */
#define OSSO_ABOOK_DUMP_VCARD_STRING(type,vcs,note)     G_STMT_START {       \
        if (G_UNLIKELY (_osso_abook_debug_flags & OSSO_ABOOK_DEBUG_##type)) {\
                _osso_abook_dump_vcard_string                                \
                        (G_LOG_DOMAIN, G_STRLOC, G_STRFUNC, "[" #type "]",   \
                         OSSO_ABOOK_DEBUG_##type, (note), vcs);              \
        }                                               } G_STMT_END

/**
 * OSSO_ABOOK_DEBUG_FLAGS:
 * @type: the required debugging flag, see #OssoABookDebugFlags
 *
 * Checks whether the debug flags cover @type.
 */
#define OSSO_ABOOK_DEBUG_FLAGS(type)                                         \
        G_UNLIKELY (_osso_abook_debug_flags & OSSO_ABOOK_DEBUG_##type)

#else

#define OSSO_ABOOK_TIMER_START(...)
#define OSSO_ABOOK_TIMER_MARK(...)
#define OSSO_ABOOK_LOCAL_TIMER_START(...)
#define OSSO_ABOOK_LOCAL_TIMER_END()
#define OSSO_ABOOK_NOTE(type,format,...)
#define OSSO_ABOOK_MARK(...)
#define OSSO_ABOOK_DEBUG_FLAGS(...) (FALSE)
#define OSSO_ABOOK_DUMP_VCARD(...)
#define OSSO_ABOOK_DUMP_VCARD_STRING(...)

#endif

G_END_DECLS

#endif /* __OSSO_ABOOK_DEBUG_H_INCLUDED__ */
