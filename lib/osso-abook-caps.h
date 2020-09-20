#ifndef __OSSO_ABOOK_CAPS_H__
#define __OSSO_ABOOK_CAPS_H__

#include <telepathy-glib/telepathy-glib.h>

#include "osso-abook-types.h"

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_CAPS \
                (osso_abook_caps_get_type ())
#define OSSO_ABOOK_CAPS(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_CAPS, \
                 OssoABookCaps))
#define OSSO_ABOOK_IS_CAPS(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_CAPS))
#define OSSO_ABOOK_CAPS_GET_IFACE(obj) \
                (G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
                 OSSO_ABOOK_TYPE_CAPS, \
                 OssoABookCapsIface))

/* This typedef cannot go in osso-abook-types.h or gtk-doc will not generate
 * cross-reference links correctly */
/**
 * OssoABookCaps:
 *
 * Dummy type for the #OssoABookCaps interface.
 */
typedef struct _OssoABookCaps OssoABookCaps;

/**
 * OssoABookCapsFlags:
 * @OSSO_ABOOK_CAPS_NONE: no capabilties at all
 * @OSSO_ABOOK_CAPS_EMAIL: e-mail is supported
 * @OSSO_ABOOK_CAPS_CHAT: text-chat is supported
 * @OSSO_ABOOK_CAPS_VOICE: audio streams are supported
 * @OSSO_ABOOK_CAPS_VIDEO: video streams are supported
 * @OSSO_ABOOK_CAPS_ADDRESSBOOK: the service has server-side addressbooks
 * @OSSO_ABOOK_CAPS_CHAT_ADDITIONAL: additional chat features are supported
 * @OSSO_ABOOK_CAPS_VOICE_ADDITIONAL: additional VoIP features are supported
 * @OSSO_ABOOK_CAPS_PHONE: Phone call is supported
 * @OSSO_ABOOK_CAPS_SMS: Sending SMS is supported
 *
 * Various capabilities of communication channels.
 */
typedef enum {
        OSSO_ABOOK_CAPS_NONE             = (0),
        OSSO_ABOOK_CAPS_EMAIL            = (1 << 0),
        OSSO_ABOOK_CAPS_CHAT             = (1 << 1),
        OSSO_ABOOK_CAPS_CHAT_ADDITIONAL  = (1 << 2),
        OSSO_ABOOK_CAPS_VOICE            = (1 << 3),
        OSSO_ABOOK_CAPS_VOICE_ADDITIONAL = (1 << 4),
        OSSO_ABOOK_CAPS_VIDEO            = (1 << 5),
        OSSO_ABOOK_CAPS_PHONE            = (1 << 6),
        OSSO_ABOOK_CAPS_ADDRESSBOOK      = (1 << 7),
        OSSO_ABOOK_CAPS_IMMUTABLE_STREAMS= (1 << 8),
        OSSO_ABOOK_CAPS_SMS              = (1 << 9),

        OSSO_ABOOK_CAPS_ALL = (OSSO_ABOOK_CAPS_EMAIL | OSSO_ABOOK_CAPS_CHAT |
                               OSSO_ABOOK_CAPS_VOICE | OSSO_ABOOK_CAPS_VIDEO |
                               OSSO_ABOOK_CAPS_PHONE | OSSO_ABOOK_CAPS_SMS)
} OssoABookCapsFlags;

/**
 * OssoABookCapsIface:
 * @get_capabilities: virtual method for osso_abook_caps_get_capabilities()
 * @get_static_capabilities: virtual method for osso_abook_caps_get_static_capabilities()
 *
 * Virtual methods of the #OssoABookCaps interface.
 */
struct _OssoABookCapsIface {
        /*< private >*/
        GTypeInterface parent;

        /*< public >*/
        OssoABookCapsFlags (* get_capabilities)        (OssoABookCaps *caps);
        OssoABookCapsFlags (* get_static_capabilities) (OssoABookCaps *caps);
};

GType
osso_abook_caps_get_type                (void) G_GNUC_CONST;

OssoABookCapsFlags
osso_abook_caps_get_capabilities        (OssoABookCaps *caps);

#ifndef OSSO_ABOOK_DISABLE_DEPRECATED
G_GNUC_DEPRECATED OssoABookCapsFlags
osso_abook_caps_get_static_capabilities (OssoABookCaps *caps);
#endif /* OSSO_ABOOK_DISABLE_DEPRECATED */

OssoABookCapsFlags
osso_abook_caps_from_tp_capabilities    (TpCapabilities *caps);

OssoABookCapsFlags
osso_abook_caps_from_tp_connection      (TpConnection *connection);

OssoABookCapsFlags
osso_abook_caps_from_tp_protocol        (TpProtocol *protocol);

OssoABookCapsFlags
osso_abook_caps_from_account            (TpAccount     *account);

G_END_DECLS

#endif /* __OSSO_ABOOK_CAPS_H__ */
