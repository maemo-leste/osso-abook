#include "osso-abook-settings.h"
#include "osso-abook-utils-private.h"

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

const char *
osso_abook_settings_get_picture_folder()
{
  static gchar *images_folder = NULL;
  const char *folder = NULL;

#if 0
  if (gconf_client_get_int(
        osso_abook_get_gconf_client(),
        "/apps/camera/settings/basic-settings/storage-device", NULL) ||
      (folder = hildon_get_user_named_dir("NOKIA_MMC_CAMERA_DIR")) == 0 ||
      !g_file_test(folder, G_FILE_TEST_IS_DIR))
  {
    folder = hildon_get_user_named_dir("NOKIA_CAMERA_DIR");
#else
  {
#endif
    if (!folder || !g_file_test(folder, G_FILE_TEST_IS_DIR))
    {
      folder = g_get_user_special_dir(G_USER_DIRECTORY_PICTURES);

      if (!folder || !g_file_test(folder, G_FILE_TEST_IS_DIR))
      {
        if (!images_folder)
          images_folder = _osso_abook_get_safe_folder(".images");

        folder = images_folder;

      }
    }
  }

  if (folder && !g_file_test(folder, G_FILE_TEST_IS_DIR))
    folder = NULL;

  return folder;
}
