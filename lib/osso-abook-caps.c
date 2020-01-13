#include <glib-object.h>
#include <gtk/gtkprivate.h>

#include "osso-abook-caps.h"

#include "config.h"

typedef OssoABookCapsIface OssoABookCapsInterface;

G_DEFINE_INTERFACE(
    OssoABookCaps,
    osso_abook_caps,
    G_TYPE_OBJECT
);

static void
osso_abook_caps_default_init(OssoABookCapsIface *klass)
{
  g_object_interface_install_property(
        klass,
        g_param_spec_uint(
                 "capabilities",
                 "Capabilities",
                 "The actual capabilities",
                 0, G_MAXUINT, 0,
                 GTK_PARAM_READABLE));
}

OssoABookCapsFlags
osso_abook_caps_get_capabilities(OssoABookCaps *caps)
{
  OssoABookCapsIface *iface;

  g_return_val_if_fail(OSSO_ABOOK_IS_CAPS(caps), OSSO_ABOOK_CAPS_NONE);

  iface = OSSO_ABOOK_CAPS_GET_IFACE(caps);

  g_return_val_if_fail(iface->get_capabilities != NULL, OSSO_ABOOK_CAPS_NONE);

  return iface->get_capabilities(caps);
}

OssoABookCapsFlags
osso_abook_caps_get_static_capabilities(OssoABookCaps *caps)
{
  OssoABookCapsIface *iface;

  g_return_val_if_fail(OSSO_ABOOK_IS_CAPS(caps), OSSO_ABOOK_CAPS_NONE);

  iface = OSSO_ABOOK_CAPS_GET_IFACE(caps);

  g_return_val_if_fail(iface->get_static_capabilities != NULL,
                       OSSO_ABOOK_CAPS_NONE);

  return iface->get_static_capabilities(caps);
}
