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

#include "osso-abook-enums.h"
#include "osso-abook-marshal.h"
#include "osso-abook-contact.h"

#include "osso-abook-account-manager.h"

#include "osso-abook-contact-detail-store.h"

struct _OssoABookContactDetailStorePrivate {
	GHashTable *message_map;
	OssoABookContact *contact;
	OssoABookContactDetailFilters details_filters_flags;
	GSequence *fields;
	int sequence_is_empty;
	int probably_initialised;
};

typedef struct _OssoABookContactDetailStorePrivate
 OssoABookContactDetailStorePrivate;

G_DEFINE_TYPE_WITH_PRIVATE(OssoABookContactDetailStore,
			   osso_abook_contact_detail_store, G_TYPE_OBJECT);

enum {
	PROP_MESSAGE_MAP = 1,
	PROP_CONTACT = 2,
	PROP_FILTERS = 3,
};


enum {
  CHANGED,
  CONTACT_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {};

#define OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(store) \
                ((OssoABookContactDetailStorePrivate *)osso_abook_contact_detail_store_get_instance_private(store))

static void
osso_abook_contact_detail_store_account_changed(
    OssoABookContactDetailStore *store)
{
  g_assert(0);
}

static void
osso_abook_contact_detail_store_class_init(OssoABookContactDetailStoreClass
					   * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

#if 0
	object_class->set_property =
	    osso_abook_contact_detail_store_set_property;
	object_class->get_property =
	    osso_abook_contact_detail_store_get_property;
	object_class->constructed = osso_abook_contact_detail_store_constructed;
	object_class->dispose = osso_abook_contact_detail_store_dispose;
#else
    g_assert(0);
#endif

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
          "contact-changed", OSSO_ABOOK_TYPE_CONTACT_DETAIL_STORE, G_SIGNAL_RUN_LAST,
          0, 0, NULL, osso_abook_marshal_VOID__OBJECT_OBJECT,
          G_TYPE_NONE, 2,
          OSSO_ABOOK_TYPE_CONTACT,
          OSSO_ABOOK_TYPE_CONTACT);
}

static void
osso_abook_contact_detail_store_init(OssoABookContactDetailStore * store)
{
    OssoABookAccountManager *account_manager;

	OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(store)->probably_initialised = 1;
    account_manager = osso_abook_account_manager_get_default();

    g_signal_connect_swapped(
        account_manager,
        "account-created",
        G_CALLBACK(osso_abook_contact_detail_store_account_changed),
        store);
    g_signal_connect_swapped(
        account_manager,
        "account-changed::enabled",
        G_CALLBACK(osso_abook_contact_detail_store_account_changed),
        store);
    g_signal_connect_swapped(
        account_manager,
        "account-removed",
        G_CALLBACK(osso_abook_contact_detail_store_account_changed),
        store);
}

GSequence *
osso_abook_contact_detail_store_get_fields(OssoABookContactDetailStore *self)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_DETAIL_STORE(self), NULL);

  return OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(self)->fields;
}
