/*
 * osso-abook-errors.c
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

/**
 * SECTION:osso-abook-errors
 * @short_description: Error handling routines.
 *
 * This module describes the types of errors that can be returned by
 * libosso-abook and provides several functions for handling them
 *
 */

#include "config.h"

#include <hildon/hildon.h>

#include <errno.h>
#include <sys/statfs.h>

#include "osso-abook-debug.h"
#include "osso-abook-errors.h"
#include "osso-abook-log.h"
#include "osso-abook-util.h"

#define MIN_FREE_BYTES (8 * 1024 * 1024)

GQuark
osso_abook_error_quark()
{
  static GQuark _osso_abook_error_quark = 0;

  if (!_osso_abook_error_quark)
  {
    _osso_abook_error_quark =
      g_quark_from_static_string("osso-abook-error-quark");
  }

  return _osso_abook_error_quark;
}

void
_osso_abook_handle_gerror(GtkWindow *parent, const char *strloc, GError *error)
{
  const char *banner_text;
  GtkWidget *widget = NULL;

  if (!error)
    return;

  g_warning("%s: GError (%s) %d: %s", strloc, g_quark_to_string(error->domain),
            error->code, error->message);

  if (error->domain == GDK_PIXBUF_ERROR)
  {
    if (error->code == GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY)
      banner_text = _("addr_ib_not_enough_memory");
    else
      banner_text = _("addr_ib_cannot_load_picture");
  }
  else if (error->domain == OSSO_ABOOK_ERROR)
    banner_text = error->message;
  else
  {
    if (g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOSPC))
      banner_text = _("addr_ib_disc_full");
    else
      banner_text = _("addr_ib_system_error");
  }

  if (parent)
    widget = GTK_WIDGET(parent);

  hildon_banner_show_information(widget, NULL, banner_text);
  g_error_free(error);
}

const char *
estatus_to_string(const EBookStatus status)
{
  if (status == E_BOOK_ERROR_PERMISSION_DENIED)
    return _("addr_ib_permission_denied");

  if (status == E_BOOK_ERROR_NO_SPACE)
    return _("addr_ib_disc_full");

  return _("addr_ib_system_error");
}

void
_osso_abook_handle_estatus(GtkWindow *parent, const char *strloc,
                           EBookStatus status, EBook *book)
{
  gchar *uid;
  const char *on;
  const char *msg;

  if (status == E_BOOK_ERROR_OK)
    return;

  if (E_IS_BOOK(book))
  {
    ESource *source = e_book_get_source(book);

    uid = e_source_dup_uid(source);
    on = " on ";
  }
  else
  {
    uid = g_strdup("");
    on = "";
  }

  g_message("%s: EBook error %d%s%s", strloc, status, on, uid);
  g_free(uid);

  msg = estatus_to_string(status);
  hildon_banner_show_information(parent ? GTK_WIDGET(parent) : NULL, 0, msg);
}

gboolean
_osso_abook_check_disc_space(GtkWindow *parent, const char *strloc,
                             const char *path)
{
  gboolean disk_full = TRUE;
  struct statfs buf;

  if (!OSSO_ABOOK_DEBUG_FLAGS(DISK_SPACE))
  {
    if (!path)
      path = osso_abook_get_work_dir();

    if (statfs(path, &buf))
    {
      OSSO_ABOOK_WARN("Error while checking file system space: %s",
                      strerror(errno));
    }
    else
    {
      uint64_t min =
        (3ULL * (uint64_t)buf.f_bsize * (uint64_t)buf.f_blocks) / 100ULL;
      uint64_t avail = ((uint64_t)buf.f_bsize * (uint64_t)buf.f_bavail);

      if (min >= (MIN_FREE_BYTES + 1ULL))
        min = MIN_FREE_BYTES;

      if (avail >= min)
        disk_full = FALSE;
    }
  }

  if (disk_full)
  {
    hildon_banner_show_information(parent ? GTK_WIDGET(parent) : NULL, NULL,
                                   _("addr_ib_disc_full"));
    g_warning("%s: insufficient disk space", strloc);
  }

  return !disk_full;
}

GError *
osso_abook_error_new_from_estatus(EBookStatus status)
{
  if (status == E_BOOK_ERROR_OK)
    return NULL;

  return g_error_new_literal(OSSO_ABOOK_ERROR, 1, estatus_to_string(status));
}
