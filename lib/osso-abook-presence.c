#include <gtk/gtk.h>
#include <gtk/gtkprivate.h>
#include <telepathy-glib/connection.h>

#include <libintl.h>
#include <stdlib.h>

#include "osso-abook-presence.h"
#include "osso-abook-utils-private.h"

#include "config.h"

typedef OssoABookPresenceIface OssoABookPresenceInterface;

static const char *
  icon_by_presence_type[TP_NUM_CONNECTION_PRESENCE_TYPES] =
{
  NULL,
  "general_presence_offline",
  "general_presence_online",
  "general_presence_away",
  "general_presence_away",
  "general_presence_offline",
  "general_presence_busy",
  NULL,
  NULL
};

static GHashTable *icon_names = NULL;

G_DEFINE_INTERFACE(
  OssoABookPresence,
  osso_abook_presence,
  G_TYPE_OBJECT
);

static void
osso_abook_presence_default_init(OssoABookPresenceIface *iface)
{
  g_object_interface_install_property(
    iface,
    g_param_spec_uint(
      "presence-type",
      "Presence Type",
      "The presence type.",
      0, G_MAXUINT, 0,
      GTK_PARAM_READABLE));
  g_object_interface_install_property(
    iface,
    g_param_spec_string(
      "presence-status",
      "Presence Status",
      "The presence status.",
      NULL,
      GTK_PARAM_READABLE));
  g_object_interface_install_property(
    iface,
    g_param_spec_string(
      "presence-status-message",
      "Presence Status Message",
      "The presence status message.",
      NULL,
      GTK_PARAM_READABLE));
  g_object_interface_install_property(
    iface,
    g_param_spec_string(
      "presence-location-string",
      "Presence Location String",
      "The location information for this presence as a string. (Deprecated)",
      NULL,
      GTK_PARAM_READABLE));
}

TpConnectionPresenceType
osso_abook_presence_get_presence_type(OssoABookPresence *presence)
{
  OssoABookPresenceIface *iface;

  g_return_val_if_fail(OSSO_ABOOK_IS_PRESENCE(presence),
                       TP_CONNECTION_PRESENCE_TYPE_UNKNOWN);

  iface = OSSO_ABOOK_PRESENCE_GET_IFACE(presence);

  g_return_val_if_fail(iface->get_presence_type != NULL,
                       TP_CONNECTION_PRESENCE_TYPE_UNKNOWN);

  return iface->get_presence_type(presence);
}

const char *
osso_abook_presence_get_presence_status(OssoABookPresence *presence)
{
  OssoABookPresenceIface *iface;
  const char *rv = NULL;

  g_return_val_if_fail(OSSO_ABOOK_IS_PRESENCE(presence), NULL);

  iface = OSSO_ABOOK_PRESENCE_GET_IFACE(presence);

  if (iface->get_presence_status)
  {
    rv = iface->get_presence_status(presence);

    if (rv && !*rv)
      rv = NULL;
  }

  return rv;
}

const char *
osso_abook_presence_get_display_status(OssoABookPresence *presence)
{
  const char *status;
  const char *msgid;

  g_return_val_if_fail(OSSO_ABOOK_IS_PRESENCE(presence), NULL);

  switch (osso_abook_presence_get_presence_type(presence))
  {
    case TP_CONNECTION_PRESENCE_TYPE_OFFLINE:
    case TP_CONNECTION_PRESENCE_TYPE_HIDDEN:
      msgid = "pres_fi_status_offline";
      break;
    case TP_CONNECTION_PRESENCE_TYPE_AVAILABLE:
      msgid = "pres_fi_status_online";
      break;
    case TP_CONNECTION_PRESENCE_TYPE_AWAY:
    case TP_CONNECTION_PRESENCE_TYPE_EXTENDED_AWAY:
      msgid = "pres_bd_skype_away";
      break;
    case TP_CONNECTION_PRESENCE_TYPE_BUSY:
      msgid = "pres_fi_status_busy";
      break;
    default:
      msgid = NULL;
      break;
  }

  status = dgettext("osso-statusbar-presence", msgid);

  if (!status || !*status)
    status = dgettext("osso-statusbar-presence", "pres_fi_status_offline");

  return status;
}

const char *
osso_abook_presence_get_presence_status_message(OssoABookPresence *presence)
{
  OssoABookPresenceIface *iface;

  g_return_val_if_fail(OSSO_ABOOK_IS_PRESENCE(presence), NULL);

  iface = OSSO_ABOOK_PRESENCE_GET_IFACE(presence);

  g_return_val_if_fail(iface->get_presence_status_message != NULL, NULL);

  return iface->get_presence_status_message(presence);
}

const char *
osso_abook_presence_get_location_string(OssoABookPresence *presence)
{
  OssoABookPresenceIface *iface;

  g_return_val_if_fail(OSSO_ABOOK_IS_PRESENCE(presence), NULL);

  iface = OSSO_ABOOK_PRESENCE_GET_IFACE(presence);

  g_return_val_if_fail(iface->get_location_string != NULL, NULL);

  return iface->get_location_string(presence);
}

__attribute__((destructor)) static void
icon_names_hash_destroy()
{
  if (icon_names)
  {
    g_hash_table_unref(icon_names);
    icon_names = NULL;
  }
}

static void
create_icon_names_hash()
{
  if (!icon_names)
  {
    icon_names = g_hash_table_new_full(g_str_hash, g_str_equal,
                                       (GDestroyNotify)&g_free,
                                       (GDestroyNotify)&g_free);
  }
}

const char *
osso_abook_presence_get_icon_name(OssoABookPresence *presence)
{
  const char *presence_status;
  gchar *icon_name = NULL;
  TpConnectionPresenceType presence_type;

  g_return_val_if_fail(OSSO_ABOOK_IS_PRESENCE(presence), NULL);

  presence_status = osso_abook_presence_get_presence_status(presence);

  if (presence_status)
  {
    if (!icon_names ||
        !(icon_name = g_hash_table_lookup(icon_names, presence_status)))
    {
      gchar *icon_name =
        g_strconcat("general_presence_", presence_status, NULL);

      if (gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), icon_name))
      {
        create_icon_names_hash();
        g_hash_table_insert(icon_names, g_strdup(presence_status), icon_name);
      }
      else
      {
        g_free(icon_name);
        icon_name = NULL;
      }
    }
  }

  if (icon_name)
    return icon_name;

  presence_type = osso_abook_presence_get_presence_type(presence);

  if (presence_type > (G_N_ELEMENTS(icon_by_presence_type) - 1))
    presence_type = TP_CONNECTION_PRESENCE_TYPE_UNKNOWN;

  return icon_by_presence_type[presence_type];
}

OssoABookPresenceState
osso_abook_presence_get_blocked(OssoABookPresence *presence)
{
  OssoABookPresenceIface *iface;

  g_return_val_if_fail(OSSO_ABOOK_IS_PRESENCE(presence),
                       OSSO_ABOOK_PRESENCE_STATE_NO);

  iface = OSSO_ABOOK_PRESENCE_GET_IFACE(presence);

  if (iface->get_blocked)
    return iface->get_blocked(presence);

  return OSSO_ABOOK_PRESENCE_STATE_NO;
}

OssoABookPresenceState
osso_abook_presence_get_published(OssoABookPresence *presence)
{
  OssoABookPresenceIface *iface;

  g_return_val_if_fail(OSSO_ABOOK_IS_PRESENCE(presence),
                       OSSO_ABOOK_PRESENCE_STATE_NO);

  iface = OSSO_ABOOK_PRESENCE_GET_IFACE(presence);

  if (iface->get_published)
    return iface->get_published(presence);

  return OSSO_ABOOK_PRESENCE_STATE_NO;
}

OssoABookPresenceState
osso_abook_presence_get_subscribed(OssoABookPresence *presence)
{
  OssoABookPresenceIface *iface;

  g_return_val_if_fail(OSSO_ABOOK_IS_PRESENCE(presence),
                       OSSO_ABOOK_PRESENCE_STATE_NO);

  iface = OSSO_ABOOK_PRESENCE_GET_IFACE(presence);

  if (iface->get_subscribed)
    return iface->get_subscribed(presence);

  return OSSO_ABOOK_PRESENCE_STATE_NO;
}

unsigned int
osso_abook_presence_get_handle(OssoABookPresence *presence)
{
  OssoABookPresenceIface *iface;

  g_return_val_if_fail(OSSO_ABOOK_IS_PRESENCE(presence), 0);

  iface = OSSO_ABOOK_PRESENCE_GET_IFACE(presence);

  if (iface->get_handle)
    return iface->get_handle(presence);

  return 0;
}

gboolean
osso_abook_presence_is_invalid(OssoABookPresence *presence)
{
  OssoABookPresenceIface *iface;

  g_return_val_if_fail(OSSO_ABOOK_IS_PRESENCE(presence), TRUE);

  iface = OSSO_ABOOK_PRESENCE_GET_IFACE(presence);

  if (iface->is_invalid)
    return iface->is_invalid(presence);

  return FALSE;
}

static TpConnectionPresenceType
custom_presence_convert(TpConnectionPresenceType presence_type)
{
  switch (presence_type)
  {
    case TP_CONNECTION_PRESENCE_TYPE_UNSET:
    case TP_CONNECTION_PRESENCE_TYPE_UNKNOWN:
    case TP_CONNECTION_PRESENCE_TYPE_ERROR:
      return TP_CONNECTION_PRESENCE_TYPE_UNKNOWN;

    default:
      return default_presence_convert(presence_type);
  }
}

static int
osso_abook_presence_compare_custom(OssoABookPresence *a,
                                   OssoABookPresence *b,
                                   TpConnectionPresenceType (*presence_convert_fn)(
                                     TpConnectionPresenceType))
{
  g_return_val_if_fail(OSSO_ABOOK_IS_PRESENCE(a), a == b ? 0 : -1);
  g_return_val_if_fail(OSSO_ABOOK_IS_PRESENCE(b), a == b ? 0 : 1);

  if (!presence_convert_fn)
    presence_convert_fn = default_presence_convert;

  return tp_connection_presence_type_cmp_availability(
    presence_convert_fn(osso_abook_presence_get_presence_type(b)),
    presence_convert_fn(osso_abook_presence_get_presence_type(a)));
}

int
osso_abook_presence_compare_for_display(OssoABookPresence *a,
                                        OssoABookPresence *b)
{
  return osso_abook_presence_compare_custom(a, b, custom_presence_convert);
}

int
osso_abook_presence_compare(OssoABookPresence *a, OssoABookPresence *b)
{
  return osso_abook_presence_compare_custom(a, b, NULL);
}
