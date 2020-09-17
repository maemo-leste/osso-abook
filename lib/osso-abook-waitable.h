#ifndef __OSSO_ABOOK_WAITABLE_H_INCLUDED__
#define __OSSO_ABOOK_WAITABLE_H_INCLUDED__

#include "osso-abook-types.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_WAITABLE \
                (osso_abook_waitable_get_type ())
#define OSSO_ABOOK_WAITABLE(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_WAITABLE, \
                 OssoABookWaitable))
#define OSSO_ABOOK_IS_WAITABLE(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_WAITABLE))
#define OSSO_ABOOK_WAITABLE_GET_IFACE(obj) \
                (G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
                 OSSO_ABOOK_TYPE_WAITABLE, \
                 OssoABookWaitableIface))

/* This typedef cannot go in osso-abook-types.h or gtk-doc will not generate
 * cross-reference links correctly */
/**
 * OssoABookWaitable:
 *
 * Dummy type for the #OssoABookWaitableIface
 */
typedef struct _OssoABookWaitable OssoABookWaitable;

#define OSSO_ABOOK_WAITABLE_CALLBACK(callback) \
        ((OssoABookWaitableCallback)(callback))

/**
 * OssoABookWaitableCallback:
 * @waitable: a #OssoABookWaitable that became ready
 * @error: %NULL on success, or a #GError to report error status
 * @data: additional data registered with osso_abook_waitable_call_when_ready()
 *
 * The type of function that is called when a #OssoABookWaitable object becomes
 * ready.
 */
typedef void (* OssoABookWaitableCallback) (OssoABookWaitable *waitable,
                                            const GError      *error,
                                            gpointer           data);

/**
 * OssoABookWaitableIface:
 * @is_ready: virtual method for osso_abook_waitable_is_ready()
 * @push: virtual method for pushing a OssoABookWaitableClosure onto the stack
 * @pop: virtual method for poppping a OssoABookWaitableClosure from the stack
 *
 * Virtual methods of the #OssoABookWaitable interface.
 */
struct _OssoABookWaitableIface {
        /*< private >*/
        GTypeInterface g_iface;

        /*< public >*/
        gboolean                   (* is_ready) (OssoABookWaitable         *waitable,
                                                 const GError             **error);

        void                       (* push)     (OssoABookWaitable         *waitable,
                                                 OssoABookWaitableClosure  *closure);

        OssoABookWaitableClosure * (* pop)      (OssoABookWaitable         *waitable,
                                                 OssoABookWaitableClosure  *closure);
};

GType
osso_abook_waitable_get_type         (void) G_GNUC_CONST;

OssoABookWaitableClosure *
osso_abook_waitable_call_when_ready  (OssoABookWaitable         *waitable,
                                      OssoABookWaitableCallback  callback,
                                      gpointer                   user_data,
                                      GDestroyNotify             destroy);

gboolean
osso_abook_waitable_cancel           (OssoABookWaitable         *waitable,
                                      OssoABookWaitableClosure  *closure);

void
osso_abook_waitable_run              (OssoABookWaitable         *waitable,
                                      GMainContext              *context,
                                      GError                   **error);

void
osso_abook_waitable_notify           (OssoABookWaitable         *waitable,
                                      const GError              *error);

void
osso_abook_waitable_reset            (OssoABookWaitable         *waitable);

gboolean
osso_abook_waitable_is_ready         (OssoABookWaitable         *waitable,
                                      GError                   **error);

G_END_DECLS

#endif /* __OSSO_ABOOK_WAITABLE_H_INCLUDED__ */
