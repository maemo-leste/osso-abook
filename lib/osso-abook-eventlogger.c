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

#include "config.h"

#include <rtcom-eventlogger/eventlogger.h>

#include "osso-abook-account-manager.h"
#include "osso-abook-eventlogger.h"
#include "osso-abook-string-list.h"
#include "osso-abook-utils-private.h"

static RTComEl *el = NULL;
static GList *el_uids = NULL;
static GList *el_contacts = NULL;
static GHashTable *vcf_attribute_uid = NULL;

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

static void
osso_abook_sync_lists(GList *old_list, GList *new_list,
                      GList **to_add, GList **to_remove,
                      GCompareFunc cmp_func, GDestroyNotify destroy_notify)
{
  GList *_to_add = NULL;
  GList *_to_remove = NULL;

  g_return_if_fail(to_add != NULL);
  g_return_if_fail(to_remove != NULL);
  g_return_if_fail(cmp_func != NULL);

  old_list = g_list_sort(old_list, cmp_func);
  new_list = g_list_sort(new_list, cmp_func);

  while (old_list && new_list)
  {
    int cmp_res = cmp_func(old_list->data, new_list->data);

    if (cmp_res > 0)
    {
      GList *l = g_list_remove_link(new_list, new_list);

      _to_add = g_list_concat(new_list, _to_add);
      new_list = l;
    }
    else if (cmp_res < 0)
    {
      GList *l = g_list_remove_link(old_list, old_list);

      _to_remove = g_list_concat(old_list, _to_remove);
      old_list = l;
    }
    else
    {
      if (destroy_notify)
      {
        destroy_notify(old_list->data);
        destroy_notify(new_list->data);
      }

      old_list = g_list_delete_link(old_list, old_list);
      new_list = g_list_delete_link(new_list, new_list);
    }
  }

  *to_add = g_list_concat(new_list, _to_add);
  *to_remove = g_list_concat(old_list, _to_remove);
}

static GList *
contact_get_attribute_values(OssoABookContact *contact, const char *attr_name)
{
  GList *attr_values = NULL;
  GList *attr;

  if (!contact)
    return NULL;

  for (attr = e_vcard_get_attributes(E_VCARD(contact)); attr; attr = attr->next)
  {
    if (!g_strcmp0(e_vcard_attribute_get_name(attr->data), attr_name))
    {
      attr_values = g_list_concat(
        g_list_copy(e_vcard_attribute_get_values(attr->data)), attr_values);
    }
  }

  return attr_values;
}

static gchar *
vcard_field_table_add_uid(const gchar *attr_val, const gchar *uid)
{
  GList *uids;
  gpointer orig_attr_val;

  if (!vcf_attribute_uid)
  {
    vcf_attribute_uid = g_hash_table_new_full(
      g_str_hash, g_str_equal, g_free,
      (GDestroyNotify)osso_abook_string_list_free);
  }

  if (g_hash_table_lookup_extended(vcf_attribute_uid, attr_val, &orig_attr_val,
                                   (gpointer *)&uids))
  {
    if (!osso_abook_string_list_find(uids, uid))
    {
      uids = g_list_prepend(uids, g_strdup(uid));
      g_hash_table_steal(vcf_attribute_uid, orig_attr_val);
      g_hash_table_insert(vcf_attribute_uid, orig_attr_val, uids);
    }

    if (uids && !uids->next)
      return uids->data;

    return NULL;
  }

  uids = g_list_prepend(NULL, g_strdup(uid));
  g_hash_table_insert(vcf_attribute_uid, g_strdup(attr_val), uids);

  return uids->data;
}

static const char *
vcard_field_table_find_uid(const char *attr_val, const char *uid)
{
  GList *l;
  GList *uids;
  gpointer orig_attr_val;

  if (!vcf_attribute_uid)
    return NULL;

  if (!g_hash_table_lookup_extended(vcf_attribute_uid, attr_val, &orig_attr_val,
                                    (gpointer *)&uids))
  {
    return NULL;
  }

  if ((l = osso_abook_string_list_find(uids, uid)))
  {
    g_free(l->data);
    uids = g_list_delete_link(uids, l);
    g_hash_table_steal(vcf_attribute_uid, orig_attr_val);
    g_hash_table_insert(vcf_attribute_uid, orig_attr_val, uids);
  }

  if (uids && !uids->next)
    return uids->data;

  return NULL;
}

void
_osso_abook_eventlogger_update(OssoABookContact *new_contact,
                               OssoABookContact *old_contact)
{
  static GList *vcard_fields = NULL;
  const char *uid;
  GList *vcf;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT(new_contact));
  g_return_if_fail(old_contact == NULL || OSSO_ABOOK_IS_CONTACT(old_contact));

  if (!_osso_abook_is_addressbook())
    return;

  uid = e_contact_get_const(E_CONTACT(new_contact), E_CONTACT_UID);

#if 0

  if (!vcard_fields)
  {
    GList *l;

    for (l = mc_profiles_list(); l; l = g_list_delete_link(l, l))
    {
      if (!(mc_profile_get_capabilities(l->data) &
            MC_PROFILE_CAPABILITY_SUPPORTS_ROSTER))
      {
        const char *vcard_field = mc_profile_get_vcard_field(l->data);

        if (vcard_field &&
            !g_list_find_custom(vcard_fields, vcard_field,
                                (GCompareFunc)g_strcmp0))
        {
          vcard_fields = g_list_prepend(vcard_fields, g_strdup(vcard_field));
        }
      }

      g_object_unref(l->data);
    }
  }

#else
  g_assert(0);
#endif

  for (vcf = vcard_fields; vcf; vcf = vcf->next)
  {
    GList *accounts;
    GList *account;

    accounts = osso_abook_account_manager_list_by_vcard_field(NULL, vcf->data);

    if (accounts)
    {
      gboolean is_tel = !g_strcmp0(vcf->data, EVC_TEL);
      GList *old_list = contact_get_attribute_values(old_contact, vcf->data);
      GList *new_list = contact_get_attribute_values(new_contact, vcf->data);
      GList *to_remove;
      GList *to_add;

      osso_abook_sync_lists(old_list, new_list, &to_add, &to_remove,
                            (GCompareFunc)&g_strcmp0, NULL);

      for (account = accounts; account; account = account->next)
      {
        const char *path_suffix = tp_account_get_path_suffix(account->data);
        GList *l;

        for (l = to_add; l; l = l->next)
        {
          if (is_tel)
          {
            const char *_uid = vcard_field_table_add_uid(l->data, uid);

            _osso_abook_eventlogger_append(path_suffix, l->data, _uid, NULL);
          }
          else
            _osso_abook_eventlogger_append(path_suffix, l->data, uid, NULL);
        }

        for (l = to_remove; l; l = l->next)
        {
          if (is_tel)
          {
            const char *_uid = vcard_field_table_find_uid(l->data, uid);

            _osso_abook_eventlogger_append(path_suffix, l->data, _uid, NULL);
          }
          else
            _osso_abook_eventlogger_append(path_suffix, l->data, NULL, NULL);
        }
      }

      g_list_free(accounts);
      g_list_free(to_add);
      g_list_free(to_remove);
    }
  }
}

void
_osso_abook_eventlogger_remove(OssoABookContact *contact)
{
  g_return_if_fail(OSSO_ABOOK_IS_CONTACT(contact));

  if (_osso_abook_is_addressbook())
  {
    const char *uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);
    GList *vals;

    for (vals = contact_get_attribute_values(contact, EVC_TEL); vals;
         vals = g_list_delete_link(vals, vals))
    {
      const char *vcf = vcard_field_table_find_uid(vals->data, uid);

      if (vcf)
      {
        _osso_abook_eventlogger_append("ring/tel/account0", vals->data, vcf,
                                       NULL);
      }
    }

    el_uids = g_list_prepend(el_uids, g_strdup(uid));
  }
}

void
_osso_abook_eventlogger_update_phone_table(OssoABookContact *contact)
{
  GList *v;

  if (_osso_abook_is_addressbook())
  {
    const char *uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);

    for (v = contact_get_attribute_values(contact, EVC_TEL); v;
         v = g_list_delete_link(v, v))
    {
      vcard_field_table_add_uid(v->data, uid);
    }
  }
}
