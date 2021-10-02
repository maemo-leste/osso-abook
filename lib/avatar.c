/*
 * avatar.c
 *
 * Copyright (C) 2021 Ivaylo Dimitrov <ivo.g.dimitrov.75@gmail.com>
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

#include "config.h"

#include "osso-abook-contact.h"
#include "osso-abook-utils-private.h"
#include "osso-abook-log.h"

#include "avatar.h"

#define MAX_AVATAR_SIZE 512000

struct _avatar_data
{
  char *data;
  gsize len;
  char *path;
  char *mime_type;
};

__attribute__ ((visibility("hidden"))) gboolean
_osso_abook_avatar_read_file(GFile *file, char **contents, gsize *length,
                             GError **error)
{
  gchar *uri;

  g_return_val_if_fail(file, FALSE);
  g_return_val_if_fail(contents, FALSE);

  uri = g_file_get_uri(file);

  if (_is_local_file(uri))
  {
    GFileInfo *info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                        G_FILE_QUERY_INFO_NONE, NULL, error);

    if (info)
    {
      if (g_file_info_get_size(info) <= MAX_AVATAR_SIZE)
      {
        if (g_file_load_contents(file, NULL, contents, length, NULL, error))
        {
          g_object_unref(info);
          g_free(uri);
          return TRUE;
        }
      }
      else
      {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                    "File '%s' is too big for an avatar", uri);
      }
    }

    g_object_unref(info);
  }

  g_free(uri);

  *contents = NULL;

  if (length)
    *length = 0;

  return FALSE;
}

__attribute__ ((visibility("hidden"))) EContactPhoto *
_osso_abook_avatar_data_to_photo(avatar_data *avatar)
{
  EContactPhoto *photo;
  guchar *data;

  g_return_val_if_fail(avatar, NULL);
  g_return_val_if_fail(avatar->data, NULL);
  g_return_val_if_fail(avatar->mime_type, NULL);

  photo = g_new(EContactPhoto, 1);
  photo->type = E_CONTACT_PHOTO_TYPE_INLINED;
  photo->data.inlined.mime_type = g_strdup(avatar->mime_type);
  data = g_new(guchar, avatar->len);
  photo->data.inlined.data = data;
  photo->data.inlined.length = avatar->len;
  memcpy(data, avatar->data, avatar->len);

  return photo;
}

__attribute__ ((visibility("hidden"))) avatar_data *
_osso_abook_avatar_data_new_from_contact(EContact *ec, gboolean only_uri)
{
  EContactPhoto *photo = osso_abook_contact_get_contact_photo(ec);
  avatar_data *data = NULL;

  if (!photo)
    return NULL;

  if (photo->type == E_CONTACT_PHOTO_TYPE_INLINED)
  {
    if (!only_uri)
    {
      data = _osso_abook_avatar_data_new();
      data->mime_type = g_strdup(photo->data.inlined.mime_type);
      data->len = photo->data.inlined.length;
      data->data = g_malloc(data->len);

      memcpy(data->data, photo->data.inlined.data, data->len);
    }
  }
  else if (photo->type == E_CONTACT_PHOTO_TYPE_URI)
    data = _osso_abook_avatar_data_new_from_uri(photo->data.inlined.mime_type);

  e_contact_photo_free(photo);

  return data;
}

__attribute__ ((visibility("hidden"))) avatar_data *
_osso_abook_avatar_data_new_from_uri(const char *uri)
{
  avatar_data *data = _osso_abook_avatar_data_new();
  GFile *file = g_file_new_for_uri(uri);
  char *content_type;
  GError *error = NULL;

  data->path = g_file_get_path(file);

  if (_osso_abook_avatar_read_file(file, &data->data, &data->len, &error))
  {
    content_type = g_content_type_guess(data->path, (guchar *)data->data,
                                        data->len, NULL);

    if (content_type)
    {
      data->mime_type = g_content_type_get_mime_type(content_type);
      g_free(content_type);
    }
    else
    {
      OSSO_ABOOK_WARN("Cannot guess content type for '%s'", uri);
      g_clear_error(&error);
    }
  }
  else
  {
    OSSO_ABOOK_WARN("Cannot read image at '%s': %s", uri, error->message);
    g_clear_error(&error);
  }

  if (!data->mime_type)
  {
    _osso_abook_avatar_data_free(data);
    data = NULL;
  }

  g_object_unref(file);

  return data;
}

__attribute__ ((visibility("hidden"))) void
_osso_abook_avatar_data_free(avatar_data *avatar)
{
  if (avatar)
  {
    g_free(avatar->data);
    g_free(avatar->path);
    g_free(avatar->mime_type);
    g_slice_free(avatar_data, avatar);
  }
}

__attribute__ ((visibility("hidden"))) avatar_data *
_osso_abook_avatar_data_new()
{
  return g_slice_new0(avatar_data);
}
