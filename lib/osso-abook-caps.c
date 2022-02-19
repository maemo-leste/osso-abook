#include "config.h"

#include <glib-object.h>
#include <gtk/gtkprivate.h>

#include "osso-abook-caps.h"

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

  /* FIXME - revisit - shall we return offline caps from the protocol? */
  if (connection)
    caps = osso_abook_caps_from_tp_connection(connection);

  return caps;
}

OssoABookCapsFlags
osso_abook_caps_from_tp_protocol(TpProtocol *protocol)
{
  OssoABookCapsFlags caps = OSSO_ABOOK_CAPS_NONE;
  GHashTable *props = NULL;
  const GValue *value;
  TpCapabilities *capabilities;

  g_object_get(protocol, "protocol-properties", &props, NULL);

  if (props)
  {
    value = tp_asv_lookup(props, TP_PROP_PROTOCOL_CONNECTION_INTERFACES);

    if (value && G_VALUE_HOLDS(value, G_TYPE_STRV))
    {
      const gchar *const *strv = g_value_get_boxed(value);

      if (strv && g_strv_contains(strv,
                                  TP_IFACE_CONNECTION_INTERFACE_CONTACT_LIST))
      {
        caps |= OSSO_ABOOK_CAPS_ADDRESSBOOK;
      }
    }

    g_hash_table_destroy(props);
  }

  capabilities = tp_protocol_get_capabilities(protocol);

  if (capabilities)
    caps |= osso_abook_caps_from_tp_capabilities(capabilities);

  return caps;
}
