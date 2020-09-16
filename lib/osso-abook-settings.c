#include "osso-abook-settings.h"

static GConfClient *gconf = NULL;

GConfClient *
osso_abook_get_gconf_client(void)
{
  if (!gconf)
  {
    gconf = gconf_client_get_default();
    gconf_client_add_dir(gconf, "/apps/osso-addressbook",
                         GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);
  }

  return gconf;
}

OssoABookNameOrder
osso_abook_settings_get_name_order(void)
{
  GConfValue *val = gconf_client_get(osso_abook_get_gconf_client(),
                                     OSSO_ABOOK_SETTINGS_KEY_NAME_ORDER, NULL);
  OssoABookNameOrder order = OSSO_ABOOK_NAME_ORDER_FIRST;

  if (val)
  {
    order = gconf_value_get_int(val);
    gconf_value_free(val);
  }

  return order;
}

OssoABookContactOrder
osso_abook_settings_get_contact_order(void)
{
  GConfValue *val = gconf_client_get(osso_abook_get_gconf_client(),
                                     OSSO_ABOOK_SETTINGS_KEY_CONTACT_ORDER,
                                     NULL);
  OssoABookContactOrder order = OSSO_ABOOK_CONTACT_ORDER_NAME;

  if (val)
  {
    order = gconf_value_get_int(val);
    gconf_value_free(val);
  }

  return order;
}

gboolean
osso_abook_settings_get_sms_button()
{
  return gconf_client_get_bool(osso_abook_get_gconf_client(),
                               OSSO_ABOOK_SETTINGS_KEY_SMS_BUTTON, FALSE);
}
