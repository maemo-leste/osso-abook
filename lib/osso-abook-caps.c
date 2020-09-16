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
osso_abook_caps_default_init(OssoABookCapsIface *iface)
{
  g_object_interface_install_property(
        iface,
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

OssoABookCapsFlags
osso_abook_caps_from_tp_capabilities(TpCapabilities *caps)
{
  OssoABookCapsFlags rv = OSSO_ABOOK_CAPS_NONE;

  g_return_val_if_fail(TP_IS_CAPABILITIES(caps), rv);

  if (tp_capabilities_supports_text_chats(caps))
    rv |= OSSO_ABOOK_CAPS_CHAT;

  if (tp_capabilities_supports_sms(caps))
    rv |= OSSO_ABOOK_CAPS_SMS;

  if (tp_capabilities_supports_audio_call(caps, TP_HANDLE_TYPE_CONTACT))
    rv |= OSSO_ABOOK_CAPS_VOICE;

  if (tp_capabilities_supports_audio_video_call(caps, TP_HANDLE_TYPE_CONTACT))
    rv |= (OSSO_ABOOK_CAPS_VOICE | OSSO_ABOOK_CAPS_VIDEO);

  return rv;
}

OssoABookCapsFlags
osso_abook_caps_from_tp_connection(TpConnection *connection)
{
  OssoABookCapsFlags rv = OSSO_ABOOK_CAPS_NONE;
  TpCapabilities *caps;

  g_return_val_if_fail(TP_IS_CONNECTION(connection), rv);
  g_return_val_if_fail(
        tp_proxy_is_prepared(connection, TP_CONNECTION_FEATURE_CAPABILITIES),
        rv);

  caps = tp_connection_get_capabilities(connection);

  rv = osso_abook_caps_from_tp_capabilities(caps);

  if (tp_proxy_is_prepared(connection, TP_CONNECTION_FEATURE_CONTACT_LIST))
    rv |= OSSO_ABOOK_CAPS_ADDRESSBOOK;

  return rv;
}

OssoABookCapsFlags
osso_abook_caps_from_account(TpAccount *account)
{
  OssoABookCapsFlags caps = OSSO_ABOOK_CAPS_NONE;
  TpConnection *connection = tp_account_get_connection(account);

  if (connection)
    caps |= osso_abook_caps_from_tp_connection(connection);

  return caps;
}
