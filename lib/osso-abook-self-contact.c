/*
 * osso-abook-self-contact.c
 *
 * Copyright (C) 2022 Ivaylo Dimitrov <ivo.g.dimitrov.75@gmail.com>
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

#include "osso-abook-account-manager.h"
#include "osso-abook-log.h"
#include "osso-abook-roster-manager.h"
#include "osso-abook-roster.h"
#include "osso-abook-util.h"
#include "osso-abook-utils-private.h"
#include "tp-glib-enums.h"

#include "osso-abook-self-contact.h"

/* FIXME - this is defined in modest headers, but we can't include because there
 * will be circular dependency */
#define MODEST_ACCOUNT_NAMESPACE "/apps/modest/accounts"

struct _OssoABookSelfContactPrivate
{
  OssoABookAccountManager *manager;
  GHashTable *accounts;
  guint gconf_handler;
};

typedef struct _OssoABookSelfContactPrivate OssoABookSelfContactPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookSelfContact,
  osso_abook_self_contact,
  OSSO_ABOOK_TYPE_GCONF_CONTACT
);

#define PRIVATE(contact) \
  ((OssoABookSelfContactPrivate *) \
   osso_abook_self_contact_get_instance_private( \
     (OssoABookSelfContact *)contact))

static const char *
osso_abook_self_contact_get_gconf_key(OssoABookGconfContact *self)
{
  return OSSO_ABOOK_SETTINGS_KEY_SELF_CARD;
}

static void
set_presence(OssoABookContact *contact, TpAccount *account)
{
  TpConnectionPresenceType type;
  gchar *message = NULL;
  gchar *status = NULL;

  type = tp_account_get_current_presence(account, &status, &message);

  if (type == TP_CONNECTION_PRESENCE_TYPE_UNSET)
  {
    TpConnection *c = tp_account_get_connection(account);

    if (c &&
        (tp_connection_get_status(c, NULL) == TP_CONNECTION_STATUS_CONNECTED))
    {
      g_free(message);
      g_free(status);
      message = NULL;
      type = TP_CONNECTION_PRESENCE_TYPE_OFFLINE;
      status = g_strdup(tp_connection_presence_type_get_nick(type));
    }
    else
      type = tp_account_get_requested_presence(account, &status, &message);
  }

  osso_abook_contact_set_presence(contact, type, status, message);

  g_free(message);
  g_free(status);
}

static void
account_presence_changed_cb(TpAccount *account, guint presence, gchar *status,
                            gchar *status_message, gpointer user_data)
{
  OssoABookSelfContactPrivate *priv = PRIVATE(user_data);
  OssoABookContact *contact = g_hash_table_lookup(priv->accounts, account);

  set_presence(contact, account);
}

static void
get_avatar_ready_cb(GObject *source_object, GAsyncResult *res,
                    gpointer user_data)
{
  TpAccount *account = TP_ACCOUNT(source_object);
  GError *error = NULL;
  const GArray *avatar = tp_account_get_avatar_finish(account, res, &error);

  if (!error)
  {
    OssoABookSelfContactPrivate *priv = PRIVATE(user_data);
    OssoABookContact *contact = g_hash_table_lookup(priv->accounts, account);
    gchar *data = NULL;
    gsize len = 0;

    if (avatar)
    {
      data = avatar->data;
      len = avatar->len;
    }

    osso_abook_contact_set_photo_data(contact, data, len, NULL, NULL);
  }
  else
  {
    OSSO_ABOOK_WARN("Could not get new avatar data %s", error->message);
    g_clear_error(&error);
  }

  g_object_unref(user_data);
}

static void
account_avatar_changed_cb(TpAccount *account, gpointer user_data)
{
  tp_account_get_avatar_async(account, get_avatar_ready_cb,
                              g_object_ref(user_data));
}

static gboolean
disconnect_account_signals(gpointer key, gpointer value, gpointer user_data)
{
  g_signal_handlers_disconnect_matched(
    key, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
    0, 0, NULL, account_presence_changed_cb, user_data);
  g_signal_handlers_disconnect_matched(
    key, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
    0, 0, NULL, account_avatar_changed_cb, user_data);

  return TRUE;
}

static void
get_contact_avatar_ready_cb(GObject *source_object, GAsyncResult *res,
                            gpointer user_data)
{
  TpAccount *account = TP_ACCOUNT(source_object);
  GError *error = NULL;
  const GArray *avatar = tp_account_get_avatar_finish(account, res, &error);

  if (!error)
  {
    OssoABookContact *contact = user_data;
    gchar *data = NULL;
    gsize len = 0;

    if (avatar)
    {
      data = avatar->data;
      len = avatar->len;
    }

    osso_abook_contact_set_photo_data(contact, data, len, NULL, NULL);
  }
  else
  {
    OSSO_ABOOK_WARN("Could not get contact avatar data %s", error->message);
    g_clear_error(&error);
  }

  g_object_unref(user_data);
}

static OssoABookContact *
create_roster_contact_from_account(TpAccount *account)
{
  const gchar *vcard_field;
  const gchar *protocol_name;
  TpProtocol *protocol;
  OssoABookRoster *roster;
  OssoABookContact *contact;
  const gchar *bound_name;
  OssoABookCapsFlags caps_flags;

  bound_name = osso_abook_tp_account_get_bound_name(account);
  g_return_val_if_fail(NULL != bound_name, NULL);

  protocol_name = tp_account_get_protocol_name(account);
  g_return_val_if_fail(NULL != protocol_name, NULL);

  protocol = osso_abook_account_manager_get_account_protocol_object(NULL,
                                                                    account);
  g_return_val_if_fail(NULL != protocol, NULL);

  vcard_field = tp_protocol_get_vcard_field(protocol);
  g_return_val_if_fail(NULL != vcard_field, NULL);

  contact = osso_abook_contact_new();
  roster = osso_abook_roster_manager_get_roster(
      NULL, tp_account_get_path_suffix(account));

  if (!roster)
  {
    roster = osso_abook_roster_new(tp_account_get_path_suffix(account), NULL,
                                   vcard_field);
    g_object_set_data_full(G_OBJECT(contact), "roster", roster,
                           (GDestroyNotify)&g_object_unref);
  }

  osso_abook_contact_set_uid(contact, tp_account_get_path_suffix(account));
  osso_abook_contact_set_value(E_CONTACT(contact),
                               OSSO_ABOOK_VCA_TELEPATHY_BLOCKED, "no");
  osso_abook_contact_set_value(E_CONTACT(contact),
                               OSSO_ABOOK_VCA_TELEPATHY_SUBSCRIBED, "yes");
  osso_abook_contact_set_value(E_CONTACT(contact),
                               OSSO_ABOOK_VCA_TELEPATHY_PUBLISHED, "yes");
  EVCardAttribute *attr = e_vcard_attribute_new(NULL, vcard_field);
  osso_abook_contact_attribute_set_protocol(attr, protocol_name);

  e_vcard_add_attribute_with_value(E_VCARD(contact), attr, bound_name);
  osso_abook_contact_set_roster(contact, roster);

  caps_flags = osso_abook_caps_from_tp_protocol(protocol);

  if (caps_flags & OSSO_ABOOK_CAPS_CHAT)
  {
    osso_abook_contact_add_value(E_CONTACT(contact),
                                 OSSO_ABOOK_VCA_TELEPATHY_CAPABILITIES, NULL,
                                 "text");
  }

  if (caps_flags & OSSO_ABOOK_CAPS_VIDEO)
  {
    osso_abook_contact_add_value(E_CONTACT(contact),
                                 OSSO_ABOOK_VCA_TELEPATHY_CAPABILITIES, NULL,
                                 "video");
  }

  if (caps_flags & OSSO_ABOOK_CAPS_VIDEO)
  {
    osso_abook_contact_add_value(E_CONTACT(contact),
                                 OSSO_ABOOK_VCA_TELEPATHY_CAPABILITIES,
                                 NULL, "voice");
  }

  set_presence(contact, account);
  tp_account_get_avatar_async(account, get_contact_avatar_ready_cb,
                              g_object_ref(contact));

  return contact;
}

static void
account_created(OssoABookAccountManager *manager, TpAccount *account,
                OssoABookSelfContact *self)
{
  OssoABookSelfContactPrivate *priv = PRIVATE(self);
  gchar *vcard_field;
  OssoABookContact *contact;

  if (g_hash_table_lookup(priv->accounts, account) ||
      !strcmp(tp_account_get_cm_name(account), "ring"))
  {
    return;
  }

  vcard_field = _osso_abook_tp_account_get_vcard_field(account);

  if (vcard_field && strcmp(vcard_field, "tel"))
  {
    const gchar *account_name = osso_abook_tp_account_get_bound_name(account);
    const gchar *protocol_name;
    GList *attrs;
    EVCardAttribute *attr;

    for (attrs = e_vcard_get_attributes(E_VCARD(self)); attrs;
         attrs = attrs->next)
    {
      if (!strcmp(e_vcard_attribute_get_name(attrs->data), vcard_field) &&
          !strcmp(e_vcard_attribute_get_value(attrs->data), account_name))
      {
        break;
      }
    }

    if (attrs && attrs->data)
    {
      attr = attrs->data;
      osso_abook_contact_attribute_set_readonly(attrs->data, TRUE);
    }
    else
    {
      attr = osso_abook_gconf_contact_add_ro_attribute(
          OSSO_ABOOK_GCONF_CONTACT(self), vcard_field, account_name);
    }

    protocol_name = tp_account_get_protocol_name(account);

    if (protocol_name)
    {
      e_vcard_attribute_add_param_with_value(
        attr, e_vcard_attribute_param_new(EVC_TYPE), protocol_name);
    }
  }

  g_free(vcard_field);
  g_signal_connect(account, "presence-changed",
                   G_CALLBACK(account_presence_changed_cb), self);
  g_signal_connect(account, "avatar-changed",
                   G_CALLBACK(account_avatar_changed_cb), self);

  contact = create_roster_contact_from_account(account);

  osso_abook_contact_attach(OSSO_ABOOK_CONTACT(self), contact);

  g_hash_table_insert(priv->accounts, g_object_ref(account), contact);
}

static void
account_removed(OssoABookAccountManager *manager, TpAccount *account,
                OssoABookSelfContact *self)
{
  OssoABookSelfContactPrivate *priv = PRIVATE(self);
  OssoABookContact *contact = g_hash_table_lookup(priv->accounts, account);
  gchar *vcard_field;

  if (!contact)
    return;

  vcard_field = _osso_abook_tp_account_get_vcard_field(account);

  if (vcard_field)
  {
    if (strcmp(vcard_field, "tel"))
    {
      osso_abook_contact_remove_value(
        E_CONTACT(self), vcard_field,
        osso_abook_tp_account_get_bound_name(account));
    }
  }

  g_free(vcard_field);

  osso_abook_contact_detach(OSSO_ABOOK_CONTACT(self), contact);

  g_signal_handlers_disconnect_matched(
    account, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
    0, 0, NULL, account_presence_changed_cb, self);
  g_signal_handlers_disconnect_matched(
    account, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
    0, 0, NULL, account_avatar_changed_cb, self);
  g_hash_table_remove(priv->accounts, account);
}

static void
osso_abook_self_contact_finalize(GObject *object)
{
  OssoABookSelfContactPrivate *priv = PRIVATE(object);

  if (priv->accounts)
  {
    g_hash_table_foreach_remove(priv->accounts, disconnect_account_signals,
                                object);
    g_hash_table_unref(priv->accounts);
  }

  if (priv->manager)
  {
    g_signal_handlers_disconnect_matched(
      priv->manager, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
      0, 0, NULL, account_created, object);
    g_signal_handlers_disconnect_matched(
      priv->manager, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
      0, 0, NULL, account_removed, object);
  }

  if (priv->gconf_handler)
  {
    gconf_client_notify_remove(osso_abook_get_gconf_client(),
                               priv->gconf_handler);
  }

  G_OBJECT_CLASS(osso_abook_self_contact_parent_class)->finalize(object);
}

static void
osso_abook_self_contact_class_init(OssoABookSelfContactClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  OssoABookGconfContactClass *gconf_contact_class =
    OSSO_ABOOK_GCONF_CONTACT_CLASS(klass);

  object_class->finalize = osso_abook_self_contact_finalize;
  gconf_contact_class->get_gconf_key = osso_abook_self_contact_get_gconf_key;
}

static void
osso_abook_self_contact_init(OssoABookSelfContact *contact)
{
  e_contact_set(E_CONTACT(contact), E_CONTACT_UID, OSSO_ABOOK_SELF_CONTACT_UID);
}

OssoABookSelfContact *
osso_abook_self_contact_new()
{
  return g_object_new(OSSO_ABOOK_TYPE_SELF_CONTACT, NULL);
}

static void
add_modest_emails(OssoABookSelfContact *self)
{
  GConfClient *gconf = osso_abook_get_gconf_client();
  GSList *accounts;

  for (accounts = gconf_client_all_dirs(gconf, MODEST_ACCOUNT_NAMESPACE, NULL);
       accounts; accounts = g_slist_delete_link(accounts, accounts))
  {
    gchar *email = g_strconcat(accounts->data, "/email", NULL);
    gchar *value = gconf_client_get_string(gconf, email, NULL);

    if (value)
    {
      osso_abook_gconf_contact_add_ro_attribute(OSSO_ABOOK_GCONF_CONTACT(self),
                                                EVC_EMAIL, value);
    }

    g_free(email);
    g_free(value);
    g_free(accounts->data);
  }
}

static void
modest_account_notify(GConfClient *client, guint cnxn_id, GConfEntry *entry,
                      gpointer user_data)
{
  GList *attrs = e_vcard_get_attributes(E_VCARD(user_data));

  while (attrs)
  {
    EVCardAttribute *attr = attrs->data;

    attrs = attrs->next;

    if (!strcmp(e_vcard_attribute_get_name(attr), EVC_EMAIL) &&
        osso_abook_contact_attribute_is_readonly(attr))
    {
      e_vcard_remove_attribute(E_VCARD(user_data), attr);
    }
  }

  add_modest_emails(user_data);
  g_signal_emit_by_name(user_data, "reset");
}

static void
add_phone_numbers(OssoABookSelfContact *self)
{
  ESource *source = _osso_abook_create_source("msisdn", OSSO_ABOOK_BACKEND_SIM);
  EBook *book;
  GError *error = NULL;

  if (!source)
    return;

  book = e_book_new(source, &error);
  g_object_unref(source);

  if (!book)
  {
    OSSO_ABOOK_WARN("%s", error->message);
    g_clear_error(&error);

    return;
  }

  if (e_book_open(book, TRUE, &error))
  {
    EBookQuery *q = e_book_query_field_exists(E_CONTACT_TEL);
    GList *contacts = NULL;

    if (e_book_get_contacts(book, q, &contacts, &error))
    {
      for (; contacts; contacts = g_list_delete_link(contacts, contacts))
      {
        GList *l;

        for (l = e_contact_get(contacts->data, E_CONTACT_TEL); l;
             l = g_list_delete_link(l, l))
        {

          osso_abook_gconf_contact_add_ro_attribute(
                OSSO_ABOOK_GCONF_CONTACT(self), EVC_TEL, l->data);
          g_free(l->data);
        }

        g_object_unref(contacts->data);
      }
    }
    else
    {
      OSSO_ABOOK_WARN("%s", error->message);
      g_clear_error(&error);
    }

    g_object_unref(book);
    e_book_query_unref(q);
  }
  else
  {
    OSSO_ABOOK_WARN("%s", error->message);
    g_clear_error(&error);
  }

  g_object_unref(book);
}

OssoABookSelfContact *
osso_abook_self_contact_get_default(void)
{
  static OssoABookSelfContact *self_contact = NULL;
  GList *accounts;
  OssoABookSelfContactPrivate *priv;
  GConfClient *gconf;

  if (self_contact)
    return g_object_ref(self_contact);

  self_contact = osso_abook_self_contact_new();
  g_object_add_weak_pointer((GObject *)self_contact, (gpointer *)&self_contact);
  osso_abook_gconf_contact_load(OSSO_ABOOK_GCONF_CONTACT(self_contact));

  priv = PRIVATE(self_contact);
  priv->accounts = g_hash_table_new_full((GHashFunc)&g_direct_hash,
                                         (GEqualFunc)&g_direct_equal,
                                         (GDestroyNotify)&g_object_unref,
                                         (GDestroyNotify)&g_object_unref);

  priv->manager = osso_abook_account_manager_get_default();
  g_signal_connect(priv->manager, "account-created",
                   G_CALLBACK(account_created), self_contact);
  g_signal_connect(priv->manager, "account-removed",
                   G_CALLBACK(account_removed), self_contact);

  for (accounts = osso_abook_account_manager_list_accounts(
         priv->manager, NULL, NULL);
       accounts; accounts = g_list_delete_link(accounts, accounts))
  {
    account_created(priv->manager, accounts->data, self_contact);
  }

  gconf = osso_abook_get_gconf_client();
  gconf_client_add_dir(gconf, MODEST_ACCOUNT_NAMESPACE,
                       GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  priv->gconf_handler = gconf_client_notify_add(
        gconf, MODEST_ACCOUNT_NAMESPACE, modest_account_notify, self_contact,
        NULL, NULL);

  add_modest_emails(self_contact);
  add_phone_numbers(self_contact);

  return self_contact;
}
