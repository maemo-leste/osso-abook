/*
 * osso-abook-presence-label.c
 *
 * Copyright (C) 2021 Merlijn Wajer <merlijn@wizzup.org>
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
#include <gtk/gtkprivate.h>
#include <hildon/hildon.h>

#include "config.h"

#include "osso-abook-account-manager.h"
#include "osso-abook-contact-detail-store.h"
#include "osso-abook-contact-field.h"
#include "osso-abook-contact.h"
#include "osso-abook-enums.h"
#include "osso-abook-marshal.h"
#include "osso-abook-message-map.h"
#include "osso-abook-quarks.h"
#include "osso-abook-util.h"
#include "osso-abook-utils-private.h"

struct _OssoABookContactDetailStorePrivate
{
  GHashTable *message_map;
  OssoABookContact *contact;
  OssoABookContactDetailFilters filters;
  GSequence *fields;
  int is_empty;
  gboolean ready;
};

typedef struct _OssoABookContactDetailStorePrivate
  OssoABookContactDetailStorePrivate;

G_DEFINE_TYPE_WITH_PRIVATE(OssoABookContactDetailStore,
                           osso_abook_contact_detail_store, G_TYPE_OBJECT);

enum
{
  PROP_MESSAGE_MAP = 1,
  PROP_CONTACT = 2,
  PROP_FILTERS = 3
};

enum
{
  CHANGED,
  CONTACT_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {};

#define OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(store) \
  ((OssoABookContactDetailStorePrivate *) \
   osso_abook_contact_detail_store_get_instance_private(( \
                                                          OssoABookContactDetailStore \
                                                          *)store))

static gint
contact_field_cmp(gconstpointer a, gconstpointer b, gpointer user_data)
{
  return osso_abook_contact_field_cmp((OssoABookContactField *)a,
                                      (OssoABookContactField *)b);
}

static void
create_contact_field(OssoABookContactDetailStorePrivate *priv,
                     EVCardAttribute *attribute)
{
  OssoABookContactField *field =
    osso_abook_contact_field_new(priv->message_map, priv->contact, attribute);

  if (!field)
    return;

  if (!IS_EMPTY(osso_abook_contact_field_get_display_value(field)))
  {
    if (!priv->fields)
      priv->fields = g_sequence_new((GDestroyNotify)&g_object_unref);

    g_sequence_insert_sorted(priv->fields, field, contact_field_cmp, NULL);
    priv->is_empty = FALSE;
  }
  else
    g_object_unref(field);
}

static void
set_full_name(OssoABookContactDetailStore *store)
{
  char *primary = NULL;
  char *secondary = NULL;
  OssoABookNameOrder order = osso_abook_settings_get_name_order();
  OssoABookContactDetailStorePrivate *priv =
    OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(store);
  gchar *name;

  osso_abook_contact_get_name_components(E_CONTACT(priv->contact), order, 1,
                                         &primary, &secondary);
  name = osso_abook_concat_names(order, primary, secondary);
  g_free(primary);
  g_free(secondary);

  if (!IS_EMPTY(name))
  {
    EVCardAttribute *attr = e_vcard_attribute_new(NULL, EVC_FN);

    e_vcard_attribute_add_value(attr, name);
    create_contact_field(priv, attr);
    e_vcard_attribute_free(attr);
  }

  g_free(name);
}

static void
create_contact_fields(OssoABookContactDetailStorePrivate *priv,
                      OssoABookContact *contact, GHashTable *contacts,
                      OssoABookContactDetailFilters allow)
{
  GList *attr;
  OssoABookContactDetailFilters flgs;

  for (attr = e_vcard_get_attributes(E_VCARD(contact)); attr; attr = attr->next)
  {
    EVCardAttribute *evcattr = attr->data;
    GQuark name = g_quark_from_string(e_vcard_attribute_get_name(evcattr));

    if ((name == OSSO_ABOOK_QUARK_VCA_EMAIL) &&
        (allow & OSSO_ABOOK_CONTACT_DETAIL_EMAIL))
    {
      create_contact_field(priv, evcattr);
    }
    else if (name == OSSO_ABOOK_QUARK_VCA_TEL)
    {
      if (!osso_abook_is_fax_attribute(evcattr))
      {
        if ((allow & OSSO_ABOOK_CONTACT_DETAIL_PHONE ||
             allow & OSSO_ABOOK_CONTACT_DETAIL_SMS) &&
            (!osso_abook_settings_get_sms_button() ||
             osso_abook_is_mobile_attribute(evcattr)))
        {
          create_contact_field(priv, evcattr);
        }
      }
      else if (allow & OSSO_ABOOK_CONTACT_DETAIL_OTHERS)
        create_contact_field(priv, evcattr);
    }
    else if ((name == OSSO_ABOOK_QUARK_VCA_NICKNAME) &&
             (allow & OSSO_ABOOK_CONTACT_DETAIL_NICKNAME))
    {
      create_contact_field(priv, evcattr);
    }
    else if (((name == OSSO_ABOOK_QUARK_VCA_ADR) ||
              (name == OSSO_ABOOK_QUARK_VCA_NOTE) ||
              (name == OSSO_ABOOK_QUARK_VCA_BDAY) ||
              (name == OSSO_ABOOK_QUARK_VCA_ORG) ||
              (name == OSSO_ABOOK_QUARK_VCA_TITLE) ||
              (name == OSSO_ABOOK_QUARK_VCA_URL) ||
              (name == OSSO_ABOOK_QUARK_VCA_GENDER)) &&
             allow & OSSO_ABOOK_CONTACT_DETAIL_OTHERS)
    {
      create_contact_field(priv, evcattr);
    }
    else
    {
      GList *roster_contacts =
        osso_abook_contact_find_roster_contacts_for_attribute(
          priv->contact, evcattr);

      if (roster_contacts)
      {
        gboolean found = FALSE;
        GList *c;

        for (c = roster_contacts; c; c = c->next)
        {
          if (g_hash_table_lookup(contacts, c->data))
            found = TRUE;
          else
            g_hash_table_insert(contacts, c->data, c->data);
        }

        if (!found)
        {
          OssoABookCaps *caps = roster_contacts->data;

          if (caps && OSSO_ABOOK_IS_CAPS(caps))
          {
            OssoABookCapsFlags caps_flags =
              osso_abook_caps_get_capabilities(OSSO_ABOOK_CAPS(caps));

            flgs = priv->filters;

            if ((flgs & OSSO_ABOOK_CONTACT_DETAIL_OTHERS) ||
                ((caps_flags & OSSO_ABOOK_CAPS_CHAT) &&
                 (flgs & OSSO_ABOOK_CONTACT_DETAIL_IM_CHAT)) ||
                ((caps_flags & OSSO_ABOOK_CAPS_VOICE) &&
                 (flgs & OSSO_ABOOK_CONTACT_DETAIL_IM_VOICE)) ||
                ((caps_flags & OSSO_ABOOK_CAPS_VIDEO) &&
                 (flgs & OSSO_ABOOK_CONTACT_DETAIL_IM_VIDEO)))
            {
              create_contact_field(priv, evcattr);
            }
          }
        }

        g_list_free(roster_contacts);
      }
      else
      {
        TpProtocol *protocol =
          osso_abook_contact_attribute_get_protocol(evcattr);

        if (protocol)
        {
          OssoABookCapsFlags caps_flase =
            osso_abook_caps_from_tp_protocol(protocol);

          if (((caps_flase & OSSO_ABOOK_CAPS_CHAT) &&
               (priv->filters & OSSO_ABOOK_CONTACT_DETAIL_IM_CHAT)) ||
              ((caps_flase & OSSO_ABOOK_CAPS_VOICE) &&
               (priv->filters & OSSO_ABOOK_CONTACT_DETAIL_IM_VOICE)) ||
              ((caps_flase & OSSO_ABOOK_CAPS_VIDEO) &&
               (priv->filters & OSSO_ABOOK_CONTACT_DETAIL_IM_VIDEO)))
          {
            create_contact_field(priv, evcattr);
          }

          g_object_unref(protocol);
        }
      }
    }
  }
}

static void
account_changed(OssoABookContactDetailStore *store)
{
  OssoABookContactDetailStorePrivate *priv =
    OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(store);

  if (!priv->ready)
  {
    if (priv->fields)
    {
      g_sequence_free(priv->fields);
      priv->fields = NULL;
      priv->is_empty = TRUE;
    }

    if (priv->contact && priv->filters)
    {
      GHashTable *contacts;
      OssoABookContact *info;

      if (priv->filters & OSSO_ABOOK_CONTACT_DETAIL_FULLNAME)
        set_full_name(store);

      contacts = g_hash_table_new(g_direct_hash, g_direct_equal);
      create_contact_fields(priv, priv->contact, contacts, priv->filters);
      info = osso_abook_contact_fetch_roster_info(priv->contact);

      if (info)
      {
        create_contact_fields(
          priv, info, contacts,
          priv->filters & ~OSSO_ABOOK_CONTACT_DETAIL_NICKNAME);
        g_object_unref(info);
      }

      g_hash_table_destroy(contacts);
    }

    g_signal_emit(store, signals[CHANGED], 0);
  }
}

static void
set_message_map(OssoABookContactDetailStore *store, GHashTable *message_map)
{
  OssoABookContactDetailStorePrivate *priv =
    OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(store);

  g_assert(NULL == priv->message_map);

  if (message_map)
    priv->message_map = g_hash_table_ref(message_map);
}

static void
osso_abook_contact_detail_store_set_property(GObject *object, guint property_id,
                                             const GValue *value,
                                             GParamSpec *pspec)
{
  OssoABookContactDetailStore *store = OSSO_ABOOK_CONTACT_DETAIL_STORE(object);

  switch (property_id)
  {
    case PROP_CONTACT:
    {
      osso_abook_contact_detail_store_set_contact(
        store, OSSO_ABOOK_CONTACT(g_value_get_object(value)));
      break;
    }
    case PROP_FILTERS:
    {
      osso_abook_contact_detail_store_set_filters(store,
                                                  g_value_get_flags(value));
      break;
    }
    case PROP_MESSAGE_MAP:
    {
      set_message_map(store, g_value_get_boxed(value));
      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
    }
  }
}

static void
osso_abook_contact_detail_store_get_property(GObject *object, guint property_id,
                                             GValue *value, GParamSpec *pspec)
{
  OssoABookContactDetailStorePrivate *priv =
    OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(object);

  switch (property_id)
  {
    case PROP_CONTACT:
    {
      g_value_set_object(value, priv->contact);
      break;
    }
    case PROP_FILTERS:
    {
      g_value_set_flags(value, priv->filters);
      break;
    }
    case PROP_MESSAGE_MAP:
    {
      g_value_set_boxed(value, priv->message_map);
      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
    }
  }
}

static void
osso_abook_contact_detail_store_constructed(GObject *object)
{
  OssoABookContactDetailStore *store = OSSO_ABOOK_CONTACT_DETAIL_STORE(object);

  OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(store)->ready = FALSE;
  account_changed(store);
}

static void
destroy_contact(OssoABookContactDetailStore *store)
{
  OssoABookContactDetailStorePrivate *priv =
    OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(store);

  if (priv->contact)
  {
    OssoABookRoster *roster = osso_abook_contact_get_roster(priv->contact);

    if (roster)
      g_object_ref(roster);

    g_signal_handlers_disconnect_matched(
      priv->contact, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, store);

    g_object_unref(priv->contact);
    priv->contact = NULL;

    if (roster)
    {
      g_signal_handlers_disconnect_matched(roster, G_SIGNAL_MATCH_DATA,
                                           0, 0, NULL, NULL, store);
      g_object_unref(roster);
    }
  }
}

static void
osso_abook_contact_detail_store_dispose(GObject *object)
{
  OssoABookContactDetailStore *store = OSSO_ABOOK_CONTACT_DETAIL_STORE(object);
  OssoABookContactDetailStorePrivate *priv =
    OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(store);

  g_signal_handlers_disconnect_matched(
    osso_abook_account_manager_get_default(),
    G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, object);
  destroy_contact(store);

  if (priv->message_map)
  {
    g_hash_table_unref(priv->message_map);
    priv->message_map = NULL;
  }

  if (priv->fields)
  {
    g_sequence_free(priv->fields);
    priv->fields = NULL;
  }

  G_OBJECT_CLASS(osso_abook_contact_detail_store_parent_class)->dispose(object);
}

static void
osso_abook_contact_detail_store_class_init(
  OssoABookContactDetailStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->set_property = osso_abook_contact_detail_store_set_property;
  object_class->get_property = osso_abook_contact_detail_store_get_property;
  object_class->constructed = osso_abook_contact_detail_store_constructed;
  object_class->dispose = osso_abook_contact_detail_store_dispose;

  g_object_class_install_property(
    object_class,
    PROP_MESSAGE_MAP,
    g_param_spec_boxed(
      "message-map", "Message Map",
      "Mapping for generic message ids to context ids",
      G_TYPE_HASH_TABLE,
      GTK_PARAM_READWRITE));

  g_object_class_install_property(
    object_class, PROP_CONTACT,
    g_param_spec_object(
      "contact", "Contact",
      "The contact of which to show details",
      OSSO_ABOOK_TYPE_CONTACT,
      GTK_PARAM_READWRITE));

  g_object_class_install_property(
    object_class, PROP_FILTERS,
    g_param_spec_flags(
      "filters", "Filters",
      "The contact field filters to use",
      OSSO_ABOOK_TYPE_CONTACT_DETAIL_FILTERS,
      OSSO_ABOOK_CONTACT_DETAIL_NONE,
      GTK_PARAM_READWRITE));

  signals[CHANGED] =
    g_signal_new(
      "changed", OSSO_ABOOK_TYPE_CONTACT_DETAIL_STORE, G_SIGNAL_RUN_LAST,
      0, 0, NULL, g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  signals[CONTACT_CHANGED] =
    g_signal_new(
      "contact-changed",
      OSSO_ABOOK_TYPE_CONTACT_DETAIL_STORE,
      G_SIGNAL_RUN_LAST,
      0,
      0,
      NULL,
      osso_abook_marshal_VOID__OBJECT_OBJECT,
      G_TYPE_NONE,
      2,
      OSSO_ABOOK_TYPE_CONTACT,
      OSSO_ABOOK_TYPE_CONTACT);
}

static void
osso_abook_contact_detail_store_init(OssoABookContactDetailStore *store)
{
  OssoABookAccountManager *account_manager;

  OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(store)->ready = TRUE;
  account_manager = osso_abook_account_manager_get_default();

  g_signal_connect_swapped(
    account_manager,
    "account-created",
    G_CALLBACK(account_changed),
    store);
  g_signal_connect_swapped(
    account_manager,
    "account-changed::enabled",
    G_CALLBACK(account_changed),
    store);
  g_signal_connect_swapped(
    account_manager,
    "account-removed",
    G_CALLBACK(account_changed),
    store);
}

OssoABookContactDetailStore *
osso_abook_contact_detail_store_new(OssoABookContact *contact,
                                    OssoABookContactDetailFilters filters)
{
  g_return_val_if_fail(NULL == contact || OSSO_ABOOK_IS_CONTACT(contact), NULL);

  return g_object_new(OSSO_ABOOK_TYPE_CONTACT_DETAIL_STORE,
                      "contact", contact,
                      "filters", filters,
                      NULL);
}

GHashTable *
osso_abook_contact_detail_store_get_message_map(
  OssoABookContactDetailStore *self)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_DETAIL_STORE(self), NULL);

  return OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(self)->message_map;
}

void
osso_abook_contact_detail_store_set_message_map(
  OssoABookContactDetailStore *self,
  const OssoABookMessageMapping *message_map)
{
  OssoABookContactDetailStorePrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_DETAIL_STORE(self));
  g_return_if_fail(message_map != NULL);

  priv = OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(self);

  if (priv->message_map)
    g_hash_table_unref(priv->message_map);

  priv->message_map = osso_abook_message_map_new(message_map);

  g_object_notify(G_OBJECT(self), "message-map");
  account_changed(self);
}

OssoABookContact *
osso_abook_contact_detail_store_get_contact(OssoABookContactDetailStore *self)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_DETAIL_STORE(self), NULL);

  return OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(self)->contact;
}

void
osso_abook_contact_detail_store_set_filters(OssoABookContactDetailStore *self,
                                            OssoABookContactDetailFilters filters)
{
  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_DETAIL_STORE(self));

  OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(self)->filters = filters;
  account_changed(self);
  g_object_notify(G_OBJECT(self), "filters");
}

GSequence *
osso_abook_contact_detail_store_get_fields(OssoABookContactDetailStore *self)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_DETAIL_STORE(self), NULL);

  return OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(self)->fields;
}

gboolean
osso_abook_contact_detail_store_is_empty(OssoABookContactDetailStore *self)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_DETAIL_STORE(self), TRUE);

  return OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(self)->is_empty;
}

static void
_temporary_saved_cb(OssoABookRoster *roster, const char *temporary_uid,
                    OssoABookContact *master_contact,
                    OssoABookContactDetailStore *store)
{
  OssoABookContact *contact;

  g_return_if_fail(NULL != temporary_uid);

  contact = OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(store)->contact;

  if (contact)
  {
    const char *uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);

    if (uid && !strcmp(uid, temporary_uid))
      osso_abook_contact_detail_store_set_contact(store, master_contact);
  }
}

static void
_contacts_changed_cb(OssoABookRoster *roster, OssoABookContact **contacts,
                     OssoABookContactDetailStore *store)
{
  OssoABookContact *contact =
    OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(store)->contact;
  const char *uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);

  for (; *contacts; contacts++)
  {
    OssoABookContact *c = *contacts;

    if (!g_strcmp0(e_contact_get_const(E_CONTACT(c), E_CONTACT_UID), uid))
    {
      osso_abook_contact_detail_store_set_contact(store, c);
      break;
    }
  }
}

static void
_contacts_removed_cb(OssoABookRoster *roster, const char **uids,
                     OssoABookContactDetailStore *store)
{
  OssoABookContact *contact =
    OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(store)->contact;
  const char *uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);

  for (; *uids; uids++)
  {
    if (!g_strcmp0(*uids, uid))
    {
      osso_abook_contact_detail_store_set_contact(store, NULL);
      break;
    }
  }
}

void
osso_abook_contact_detail_store_set_contact(OssoABookContactDetailStore *self,
                                            OssoABookContact *contact)
{
  OssoABookContactDetailStorePrivate *priv;
  OssoABookContact *old_contact;
  OssoABookRoster *roster = NULL;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_DETAIL_STORE(self));
  g_return_if_fail(contact == NULL || OSSO_ABOOK_IS_CONTACT(contact));

  if (contact)
    g_object_ref(contact);

  priv = OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(self);
  old_contact = priv->contact;

  if (old_contact)
    old_contact = g_object_ref(old_contact);

  destroy_contact(self);

  if (contact)
  {
    roster = osso_abook_contact_get_roster(contact);
    priv->contact = contact;
  }

  if (roster && OSSO_ABOOK_IS_AGGREGATOR(roster))
  {
    if (osso_abook_contact_is_temporary(contact))
    {
      g_signal_connect(roster, "temporary-contact-saved",
                       G_CALLBACK(_temporary_saved_cb), self);
    }
    else
    {
      g_signal_connect(roster, "contacts-changed",
                       G_CALLBACK(_contacts_changed_cb), self);
    }

    g_signal_connect_after(roster, "contacts-removed",
                           G_CALLBACK(_contacts_removed_cb), self);
  }
  else if (contact)
  {
    g_signal_connect_swapped(contact, "reset",
                             G_CALLBACK(account_changed), self);
    g_signal_connect_swapped(contact, "contact-attached",
                             G_CALLBACK(account_changed), self);
    g_signal_connect_swapped(contact, "contact-detached",
                             G_CALLBACK(account_changed), self);
  }

  account_changed(self);

  g_object_notify(G_OBJECT(self), "contact");
  g_signal_emit(self, signals[CONTACT_CHANGED], 0, old_contact, priv->contact);

  if (old_contact)
    g_object_unref(old_contact);
}
