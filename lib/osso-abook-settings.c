/*
 * osso-abook-settings.c
 *
 * Copyright (C) 2020 Ivaylo Dimitrov <ivo.g.dimitrov.75@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <https://www.gnu.org/licenses/>.
 *
 */

/**
 * SECTION:osso-abook-settings
 * @short_description: Helper functions for reading and writing settings
 *
 * This module provides functions that allow you to access and modify the
 * settings of the addressbook
 *
 */

#include "config.h"

#include "osso-abook-errors.h"
#include "osso-abook-log.h"
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

gboolean
osso_abook_settings_get_video_button()
{
  return gconf_client_get_bool(osso_abook_get_gconf_client(),
                               OSSO_ABOOK_SETTINGS_KEY_VIDEO_BUTTON, FALSE);
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
      ((folder = hildon_get_user_named_dir("NOKIA_MMC_CAMERA_DIR")) == 0) ||
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

OssoABookVoicemailNumber *
osso_abook_voicemail_number_new(const char *phone_number,
                                const char *operator_id,
                                const char *operator_name)
{
  OssoABookVoicemailNumber *number = g_slice_new0(OssoABookVoicemailNumber);

  if (!IS_EMPTY(operator_id))
  {
    const char *p = operator_id;

    while (*p)
    {
      if (!g_ascii_isdigit(*p))
        goto bad_id;

      p++;
    }

    if (p - operator_id < 5)
    {
bad_id:
      OSSO_ABOOK_WARN("Ignoring malformed operator ID: \"%s\"",
                      operator_id);
      operator_id = NULL;
    }
  }

  number->phone_number = g_strdup(phone_number);
  number->operator_id = g_strndup(operator_id, 5);
  number->operator_name = g_strdup(operator_name);

  if (!number->operator_id || number->operator_name)
    return number;

  number->operator_name =
    _osso_abook_get_operator_name(NULL, number->operator_id, NULL);

  return number;
}

void
osso_abook_voicemail_number_free(OssoABookVoicemailNumber *number)
{
  if (number)
  {
    g_free(number->phone_number);
    g_free(number->operator_id);
    g_free(number->operator_name);
    g_slice_free(OssoABookVoicemailNumber, number);
  }
}

void
osso_abook_voicemail_number_list_free(GSList *list)
{
  g_slist_free_full(list, (GDestroyNotify)osso_abook_voicemail_number_free);
}

GSList *
osso_abook_settings_get_voicemail_numbers()
{
  GSList *numbers = gconf_client_get_list(
    osso_abook_get_gconf_client(),
    OSSO_ABOOK_SETTINGS_KEY_VOICEMAIL_NUMBERS, GCONF_VALUE_STRING, NULL);
  GSList *l;

  for (l = numbers; l; l = l->next)
  {
    const char *name = NULL;
    const char *id = NULL;
    gchar **arr = g_strsplit(l->data, ";", 3);

    if (arr[0])
    {
      id = arr[1];

      if (id)
        name = arr[2];
    }

    l->data = osso_abook_voicemail_number_new(arr[0], id, name);
    g_strfreev(arr);
  }

  return numbers;
}

gboolean
osso_abook_settings_set_voicemail_numbers(GSList *numbers)
{
  GSList *l;
  gboolean rv;
  GError *error = NULL;

  numbers = g_slist_copy(numbers);

  for (l = numbers; l; l = l->next)
  {
    OssoABookVoicemailNumber *number = l->data;

    l->data = g_strconcat(number->phone_number, ";", number->operator_id, ";",
                          number->operator_name, NULL);
  }

  rv = gconf_client_set_list(osso_abook_get_gconf_client(),
                             OSSO_ABOOK_SETTINGS_KEY_VOICEMAIL_NUMBERS,
                             GCONF_VALUE_STRING, numbers, &error);
  osso_abook_handle_gerror(NULL, error);
  g_slist_free_full(numbers, g_free);

  return rv;
}

gboolean
osso_abook_settings_set_home_applets(GSList *list)
{
  gboolean res;
  GError *error = NULL;

  res = gconf_client_set_list(osso_abook_get_gconf_client(),
                              OSSO_ABOOK_SETTINGS_KEY_HOME_APPLETS,
                              GCONF_VALUE_STRING, list, &error);
  osso_abook_handle_gerror(NULL, error);

  return res;
}

GSList *
osso_abook_settings_get_home_applets()
{
  return gconf_client_get_list(osso_abook_get_gconf_client(),
                               OSSO_ABOOK_SETTINGS_KEY_HOME_APPLETS,
                               GCONF_VALUE_STRING, NULL);
}

gboolean
osso_abook_settings_set_name_order(OssoABookNameOrder order)
{
  gboolean rv;
  GError *error = NULL;

  rv = gconf_client_set_int(osso_abook_get_gconf_client(),
                            OSSO_ABOOK_SETTINGS_KEY_NAME_ORDER, order, &error);

  osso_abook_handle_gerror(NULL, error);

  return rv;
}

gboolean
osso_abook_settings_set_contact_order(OssoABookContactOrder order)
{
  gboolean rv;
  GError *error = NULL;

  rv = gconf_client_set_int(osso_abook_get_gconf_client(),
                            OSSO_ABOOK_SETTINGS_KEY_CONTACT_ORDER, order,
                            &error);
  osso_abook_handle_gerror(NULL, error);

  return rv;
}

gboolean
osso_abook_settings_set_video_button(gboolean enabled)
{
  gboolean rv;
  GError *error = NULL;

  rv = gconf_client_set_bool(osso_abook_get_gconf_client(),
                             OSSO_ABOOK_SETTINGS_KEY_VIDEO_BUTTON, enabled,
                             &error);
  osso_abook_handle_gerror(NULL, error);

  return rv;
}

gboolean
osso_abook_settings_set_sms_button(gboolean enabled)
{
  gboolean rv;
  GError *error = NULL;

  rv = gconf_client_set_bool(osso_abook_get_gconf_client(),
                             OSSO_ABOOK_SETTINGS_KEY_SMS_BUTTON, enabled,
                             &error);
  osso_abook_handle_gerror(NULL, error);

  return rv;
}
