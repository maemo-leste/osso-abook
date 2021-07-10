/*
 * osso-abook-message-map.c
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

#include <libintl.h>

#include "config.h"

#include "osso-abook-message-map.h"
#include "osso-abook-debug.h"


GHashTable *
osso_abook_message_map_new(const OssoABookMessageMapping *mappings)
{
  GHashTable *map;
  const OssoABookMessageMapping *m;

  g_return_val_if_fail(NULL != mappings, NULL);

  map = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify)&g_free,
                              (GDestroyNotify)&g_free);

  for (m = mappings; m->generic_id; m++)
    g_hash_table_insert(map, g_strdup(m->generic_id), g_strdup(m->context_id));

  return map;
}

const char *
osso_abook_message_map_lookup(GHashTable *map, const char *msgid)
{
  gchar *msg;

  g_return_val_if_fail(NULL != map, NULL);
  g_return_val_if_fail(NULL != msgid, NULL);

  msg = g_hash_table_lookup(map, msgid);

  if (!msg)
  {
    msg = g_strconcat(g_hash_table_lookup(map, ""), "_", msgid, NULL);

    OSSO_ABOOK_NOTE(I18N, "Using fallback for message id \"%s\": \"%s\"",
                    msgid, msg);

    g_hash_table_insert(map, g_strdup(msgid), msg);
  }

  if (msg && *msg)
    return dgettext("osso-addressbook", msg);

  return NULL;
}
