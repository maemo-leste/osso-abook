#ifndef __OSSO_ABOOK_PRESENCE_H_INCLUDED__
#define __OSSO_ABOOK_PRESENCE_H_INCLUDED__

#include "osso-abook-types.h"

#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>

G_BEGIN_DECLS

/**
 * OssoABookPresenceState:
 * @OSSO_ABOOK_PRESENCE_STATE_YES: the condition is true
 * @OSSO_ABOOK_PRESENCE_STATE_NO: the condition is false
 * @OSSO_ABOOK_PRESENCE_STATE_LOCAL_PENDING: the state is not resolved yet
 * and pending on some local action, e.g. some confirmation
 * @OSSO_ABOOK_PRESENCE_STATE_REMOTE_PENDING: the state is not resolved yet
 * and pending on some remote action, e.g. network latency
 *
 * The various possible states of presence information. A simple boolean flag
 * is not sufficient for representing this information, since presence state
 * can depend on manual feedback and actual network conditions.
 */
typedef enum {
        OSSO_ABOOK_PRESENCE_STATE_YES,
        OSSO_ABOOK_PRESENCE_STATE_NO,
        OSSO_ABOOK_PRESENCE_STATE_LOCAL_PENDING,
        OSSO_ABOOK_PRESENCE_STATE_REMOTE_PENDING,
} OssoABookPresenceState;

#define OSSO_ABOOK_TYPE_PRESENCE \
                (osso_abook_presence_get_type ())
#define OSSO_ABOOK_PRESENCE(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_PRESENCE, \
                 OssoABookPresence))
#define OSSO_ABOOK_IS_PRESENCE(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_PRESENCE))
#define OSSO_ABOOK_PRESENCE_GET_IFACE(obj) \
                (G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
                 OSSO_ABOOK_TYPE_PRESENCE, \
                 OssoABookPresenceIface))

/* This typedef cannot go in osso-abook-types.h or gtk-doc will not generate
 * cross-reference links correctly */
/**
 * OssoABookPresence:
 *
 * Dummy type for the #OssoABookPresence interface.
 */
typedef struct _OssoABookPresence OssoABookPresence;

/**
 * OssoABookPresenceIface:
 * @get_presence_type: virtual method for osso_abook_presence_get_presence_type()
 * @get_presence_status: virtual method for osso_abook_presence_get_presence_status()
 * @get_presence_status_message: virtual method for osso_abook_presence_get_presence_status_message()
 * @get_location_string: virtual method for osso_abook_presence_get_location_string()
 * @get_blocked: virtual method for osso_abook_presence_get_blocked()
 * @get_published: virtual method for osso_abook_presence_get_published()
 * @get_subscribed: virtual method for osso_abook_presence_get_subscribed()
 * @get_handle: virtual method for osso_abook_presence_get_handle()
 * @is_invalid: virtual method for osso_abook_presence_is_invalid()
 *
 * Virtual methods of the #OssoABookPresence interface.
 */
struct _OssoABookPresenceIface {
        /*< private >*/
        GTypeInterface parent;

        /*< public >*/
        TpConnectionPresenceType (* get_presence_type)           (OssoABookPresence *presence);
        const char *             (* get_presence_status)         (OssoABookPresence *presence);
        const char *             (* get_presence_status_message) (OssoABookPresence *presence);
        const char *             (* get_location_string)         (OssoABookPresence *presence);

        OssoABookPresenceState   (* get_blocked)                 (OssoABookPresence *presence);
        OssoABookPresenceState   (* get_published)               (OssoABookPresence *presence);
        OssoABookPresenceState   (* get_subscribed)              (OssoABookPresence *presence);
        unsigned                 (* get_handle)                  (OssoABookPresence *presence);
        gboolean                 (* is_invalid)                  (OssoABookPresence *presence);
};

GType
osso_abook_presence_get_type                    (void) G_GNUC_CONST;

TpConnectionPresenceType
osso_abook_presence_get_presence_type           (OssoABookPresence *presence);

const char *
osso_abook_presence_get_presence_status         (OssoABookPresence *presence);

const char *
osso_abook_presence_get_display_status          (OssoABookPresence *presence);

const char *
osso_abook_presence_get_presence_status_message (OssoABookPresence *presence);

G_GNUC_DEPRECATED
const char *
osso_abook_presence_get_location_string         (OssoABookPresence *presence);

const char *
osso_abook_presence_get_icon_name               (OssoABookPresence *presence);

const char *
osso_abook_presence_get_branded_icon_name       (OssoABookPresence *presence,
                                                 TpProtocol        *protocol);

OssoABookPresenceState
osso_abook_presence_get_blocked                 (OssoABookPresence *presence);

OssoABookPresenceState
osso_abook_presence_get_published               (OssoABookPresence *presence);

OssoABookPresenceState
osso_abook_presence_get_subscribed              (OssoABookPresence *presence);

unsigned
osso_abook_presence_get_handle                  (OssoABookPresence *presence);

gboolean
osso_abook_presence_is_invalid                  (OssoABookPresence *presence);

int
osso_abook_presence_compare_for_display         (OssoABookPresence *a,
                                                 OssoABookPresence *b);

int
osso_abook_presence_compare                     (OssoABookPresence *a,
                                                 OssoABookPresence *b);

G_END_DECLS

#endif /* __OSSO_ABOOK_PRESENCE_H_INCLUDED__ */
