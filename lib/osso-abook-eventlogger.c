/*
 * osso-abook-eventlogger.c
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

#include <rtcom-eventlogger/eventlogger.h>

#include "config.h"

#include "osso-abook-eventlogger.h"
#include "osso-abook-utils-private.h"

static RTComEl *el = NULL;
static GList *el_uids = NULL;
static GList *el_contacts = NULL;

static void
rtcom_el_remote_destroy(RTComElRemote *remote)
{
  if (!remote)
    return;

  g_free(remote->local_uid);
  g_free(remote->remote_uid);
  g_free(remote->abook_uid);
  g_free(remote->remote_name);
  g_slice_free(RTComElRemote, remote);
}

void
_osso_abook_eventlogger_apply()
{
  GList *uid;
  GList *l;

  if (!_osso_abook_is_addressbook())
    return;

  if (!el)
    el = rtcom_el_get_shared();

  for (uid = el_uids; uid; uid = uid->next)
  {
    for (l = el_contacts; l; l = l->next)
    {
      RTComElRemote *remote = l->data;

      if (!g_strcmp0(uid->data, remote->abook_uid))
      {
        rtcom_el_remote_destroy(remote);
        el_contacts = g_list_delete_link(el_contacts, l);
        break;
      }
    }
  }

  if (el_contacts)
  {
    rtcom_el_update_remote_contacts(el, el_contacts, NULL);

    for (l = el_contacts; l; el_contacts = l)
    {
      rtcom_el_remote_destroy(l->data);
      l = g_list_delete_link(el_contacts, el_contacts);
    }
  }

  if (el_uids)
  {
    rtcom_el_remove_abook_uids(el, el_uids, NULL);
    g_list_free_full(el_uids, g_free);
    el_uids = NULL;
  }
}

static void
_osso_abook_eventlogger_append(const gchar *local_uid, const gchar *remote_uid,
                               const gchar *abook_uid, const gchar *remote_name)
{
  RTComElRemote *remote;

  if (el_contacts)
  {
    GList *l;

    for (l = el_contacts; l; l = l->next)
    {
      remote = l->data;

      if (!g_strcmp0(remote->local_uid, local_uid) &&
          !g_strcmp0(remote->remote_uid, remote_uid))
      {
        g_free(remote->abook_uid);
        g_free(remote->remote_name);
        remote->abook_uid = g_strdup(abook_uid);
        remote->remote_name = g_strdup(remote_name);
        return;
      }
    }
  }

  remote = g_slice_new0(RTComElRemote);
  remote->local_uid = g_strdup(local_uid);
  remote->remote_uid = g_strdup(remote_uid);
  remote->abook_uid = g_strdup(abook_uid);
  remote->remote_name = g_strdup(remote_name);

  el_contacts = g_list_prepend(el_contacts, remote);
}

void
_osso_abook_eventlogger_update_roster(TpAccount *account,
                                      const char *remote_uid,
                                      const gchar *abook_uid)
{
  g_return_if_fail(TP_IS_ACCOUNT(account));
  g_return_if_fail(!IS_EMPTY(remote_uid));

  if (_osso_abook_is_addressbook())
  {
    _osso_abook_eventlogger_append(tp_account_get_path_suffix(account),
                                   remote_uid, abook_uid, NULL);
  }
}
